// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/main/viz_main_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/message_loop/message_pump_type.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/ui_devtools/buildflags.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/passthrough_program_cache.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_init.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "media/gpu/buildflags.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "third_party/skia/include/core/SkFontLCDConfig.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include "ui/gl/gl_surface_egl.h"
#endif  // OS_WIN

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {

std::unique_ptr<base::Thread> CreateAndStartIOThread() {
  // TODO(sad): We do not need the IO thread once gpu has a separate process.
  // It should be possible to use |main_task_runner_| for doing IO tasks.
  base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
  // TODO(reveman): Remove this in favor of setting it explicitly for each
  // type of process.
  if (base::FeatureList::IsEnabled(features::kGpuUseDisplayThreadPriority))
    thread_options.priority = base::ThreadPriority::DISPLAY;
  auto io_thread = std::make_unique<base::Thread>("GpuIOThread");
  CHECK(io_thread->StartWithOptions(thread_options));
  return io_thread;
}

}  // namespace

namespace viz {

VizMainImpl::ExternalDependencies::ExternalDependencies() = default;

VizMainImpl::ExternalDependencies::~ExternalDependencies() = default;

VizMainImpl::ExternalDependencies::ExternalDependencies(
    ExternalDependencies&& other) = default;

VizMainImpl::ExternalDependencies& VizMainImpl::ExternalDependencies::operator=(
    ExternalDependencies&& other) = default;

VizMainImpl::DrDcDependencies::DrDcDependencies() = default;
VizMainImpl::DrDcDependencies::~DrDcDependencies() = default;

VizMainImpl::VizMainImpl(Delegate* delegate,
                         ExternalDependencies dependencies,
                         std::unique_ptr<gpu::GpuInit> gpu_init)
    : delegate_(delegate),
      dependencies_(std::move(dependencies)),
      gpu_init_(std::move(gpu_init)),
      gpu_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(gpu_init_);

  // TODO(crbug.com/609317): Remove this when Mus Window Server and GPU are
  // split into separate processes. Until then this is necessary to be able to
  // run Mushrome (chrome with mus) with Mus running in the browser process.
  if (!base::PowerMonitor::IsInitialized()) {
    base::PowerMonitor::Initialize(
        std::make_unique<base::PowerMonitorDeviceSource>());
  }

  if (!dependencies_.io_thread_task_runner)
    io_thread_ = CreateAndStartIOThread();
  if (dependencies_.create_display_compositor) {
    if (dependencies.viz_compositor_thread_runner) {
      viz_compositor_thread_runner_ = dependencies.viz_compositor_thread_runner;
    } else {
      viz_compositor_thread_runner_impl_ =
          std::make_unique<VizCompositorThreadRunnerImpl>();
      viz_compositor_thread_runner_ = viz_compositor_thread_runner_impl_.get();
    }
    if (delegate_) {
      delegate_->PostCompositorThreadCreated(
          viz_compositor_thread_runner_->task_runner());
    }
  }

  if (!gpu_init_->gpu_info().in_process_gpu && dependencies.ukm_recorder) {
    // NOTE: If the GPU is running in the browser process, we can use the
    // browser's UKMRecorder.
    ukm_recorder_ = std::move(dependencies.ukm_recorder);
    ukm::DelegatingUkmRecorder::Get()->AddDelegate(ukm_recorder_->GetWeakPtr());
  }

  gpu_service_ = std::make_unique<GpuServiceImpl>(
      gpu_init_->gpu_info(), gpu_init_->TakeWatchdogThread(), io_task_runner(),
      gpu_init_->gpu_feature_info(), gpu_init_->gpu_preferences(),
      gpu_init_->gpu_info_for_hardware_gpu(),
      gpu_init_->gpu_feature_info_for_hardware_gpu(),
      gpu_init_->gpu_extra_info(), gpu_init_->vulkan_implementation(),
      base::BindOnce(&VizMainImpl::ExitProcess, base::Unretained(this)));
  if (dependencies_.create_display_compositor) {
    gpu_service_->set_oopd_enabled();

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableDrDc)) {
      dr_dc_deps_.enabled = true;
      dr_dc_deps_.thread = std::make_unique<base::Thread>("DrDcThread");
      dr_dc_deps_.thread->StartWithOptions(
          base::Thread::Options(base::MessagePumpType::UI, 0));
    }
  }

#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()->AddInterfaces(&registry_);
#endif
}

VizMainImpl::~VizMainImpl() {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  // The compositor holds on to some resources from gpu service. So destroy the
  // compositor first, before destroying the gpu service. However, before the
  // compositor is destroyed, close the binding, so that the gpu service doesn't
  // need to process commands from the host as it is shutting down.
  receiver_.reset();

  // If the VizCompositorThread was started and owned by VizMainImpl, then this
  // will block until the thread has been shutdown. All RootCompositorFrameSinks
  // must be destroyed before now, otherwise the compositor thread will deadlock
  // waiting for a response from the blocked GPU thread.
  // For the non-owned case for Android WebView, Viz does not communicate with
  // this thread so there is no need to shutdown viz first.
  viz_compositor_thread_runner_ = nullptr;
  viz_compositor_thread_runner_impl_.reset();

  if (ukm_recorder_)
    ukm::DelegatingUkmRecorder::Get()->RemoveDelegate(ukm_recorder_.get());
}

void VizMainImpl::SetLogMessagesForHost(LogMessages log_messages) {
  log_messages_ = std::move(log_messages);
}

void VizMainImpl::BindAssociated(
    mojo::PendingAssociatedReceiver<mojom::VizMain> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

#if defined(USE_OZONE)
bool VizMainImpl::CanBindInterface(const std::string& interface_name) const {
  return registry_.CanBindInterface(interface_name);
}

void VizMainImpl::BindInterface(const std::string& interface_name,
                                mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}
#endif

void VizMainImpl::CreateGpuService(
    mojo::PendingReceiver<mojom::GpuService> pending_receiver,
    mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
    mojo::PendingRemote<
        discardable_memory::mojom::DiscardableSharedMemoryManager>
        discardable_memory_manager,
    mojo::ScopedSharedBufferHandle activity_flags,
    gfx::FontRenderParams::SubpixelRendering subpixel_rendering) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  mojo::Remote<mojom::GpuHost> gpu_host(std::move(pending_gpu_host));

  // If GL is disabled then don't try to collect GPUInfo, we're not using GPU.
  if (gl::GetGLImplementation() != gl::kGLImplementationDisabled)
    gpu_service_->UpdateGPUInfo();

  for (const LogMessage& log : log_messages_)
    gpu_host->RecordLogMessage(log.severity, log.header, log.message);
  log_messages_.clear();
  if (!gpu_init_->init_successful()) {
    LOG(ERROR) << "Exiting GPU process due to errors during initialization";
    gpu_service_.reset();
    gpu_host->DidFailInitialize();
    if (delegate_)
      delegate_->OnInitializationFailed();
    return;
  }

  if (!gpu_init_->gpu_info().in_process_gpu) {
    // If the GPU is running in the browser process, discardable memory manager
    // has already been initialized.
    discardable_shared_memory_manager_ = std::make_unique<
        discardable_memory::ClientDiscardableSharedMemoryManager>(
        std::move(discardable_memory_manager), io_task_runner());
    base::DiscardableMemoryAllocator::SetInstance(
        discardable_shared_memory_manager_.get());
  }

  SkFontLCDConfig::SetSubpixelOrder(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrder(
          subpixel_rendering));
  SkFontLCDConfig::SetSubpixelOrientation(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrientation(
          subpixel_rendering));

  gpu_service_->Bind(std::move(pending_receiver));
  gpu_service_->InitializeWithHost(
      gpu_host.Unbind(),
      gpu::GpuProcessActivityFlags(std::move(activity_flags)),
      gpu_init_->TakeDefaultOffscreenSurface(),
      dependencies_.sync_point_manager, dependencies_.shared_image_manager,
      dependencies_.shutdown_event);

  if (dr_dc_deps_.enabled) {
    base::WaitableEvent event;
    dr_dc_deps_.thread->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VizMainImpl::InitializeDrDcDependenciesOnThread,
                       base::Unretained(this), &event));
    event.Wait();
  }

  if (!pending_frame_sink_manager_params_.is_null()) {
    mojom::FrameSinkManagerParamsPtr params =
        std::move(pending_frame_sink_manager_params_);
    pending_frame_sink_manager_params_.reset();
    CreateFrameSinkManager(std::move(params));
  }
  if (delegate_)
    delegate_->OnGpuServiceConnection(gpu_service_.get());
}

void VizMainImpl::InitializeDrDcDependenciesOnThread(
    base::WaitableEvent* event) {
  DCHECK(dr_dc_deps_.enabled);
  DCHECK_EQ(dr_dc_deps_.thread->GetThreadId(),
            base::PlatformThread::CurrentId());

#if defined(OS_WIN)
  gl::GLSurfaceEGL::InitializeDisplay(::GetDC(nullptr));
#endif  // OS_WIN

  dr_dc_deps_.mailbox_manager =
      std::make_unique<gpu::gles2::MailboxManagerImpl>();

  auto share_group = base::MakeRefCounted<gl::GLShareGroup>();
  auto surface = gl::init::CreateOffscreenGLSurface(gfx::Size());

  const auto& gpu_preferences =
      gpu_service_->gpu_channel_manager()->gpu_preferences();
  const bool use_passthrough_decoder =
      gpu::gles2::PassthroughCommandDecoderSupported() &&
      gpu_preferences.use_passthrough_cmd_decoder;
  gl::GLContextAttribs attribs = gpu::gles2::GenerateGLContextAttribs(
      gpu::ContextCreationAttribs(), use_passthrough_decoder);

  auto context =
      gl::init::CreateGLContext(share_group.get(), surface.get(), attribs);
  DCHECK(context);
  DCHECK_EQ(context->share_group(), share_group.get());

  const auto& gpu_feature_info =
      gpu_service_->gpu_channel_manager()->gpu_feature_info();
  gpu_feature_info.ApplyToGLContext(context.get());

  bool is_current = context->MakeCurrent(surface.get());
  DCHECK(is_current);

  const gpu::GpuDriverBugWorkarounds workarounds(
      gpu_feature_info.enabled_gpu_driver_bug_workarounds);
  const bool disable_disk_cache =
      gpu_preferences.disable_gpu_shader_disk_cache ||
      workarounds.disable_program_disk_cache;
  dr_dc_deps_.program_cache = std::make_unique<gpu::gles2::MemoryProgramCache>(
      gpu_preferences.gpu_program_cache_size, disable_disk_cache,
      workarounds.disable_program_caching_for_transform_feedback,
      gpu_service_->gpu_channel_manager()->activity_flags());

  auto context_lost_callback =
      base::BindOnce(&gpu::GpuChannelManager::OnContextLost,
                     gpu_service_->gpu_channel_manager()->GetWeakPtr(),
                     /*synthetic_loss=*/false);

  // Notify GPU main thread about context loss.
  auto post_context_lost_callback = base::BindOnce(
      [](scoped_refptr<base::SingleThreadTaskRunner> task_runner,
         base::OnceClosure callback) {
        NOTREACHED();
        task_runner->PostTask(FROM_HERE, std::move(callback));
      },
      gpu_thread_task_runner_, std::move(context_lost_callback));

  dr_dc_deps_.shared_context_state =
      base::MakeRefCounted<gpu::SharedContextState>(
          std::move(share_group), std::move(surface), std::move(context),
          /*use_virtualized_gl_contexts=*/false,
          std::move(post_context_lost_callback),
          gpu_preferences.gr_context_type,
          /*vulkan_context_provider_=*/nullptr,
          /*metal_context_provider_=*/nullptr);
  event->Signal();
}

void VizMainImpl::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params) {
  DCHECK(viz_compositor_thread_runner_);
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  if (!gpu_service_ || !gpu_service_->is_initialized()) {
    DCHECK(pending_frame_sink_manager_params_.is_null());
    pending_frame_sink_manager_params_ = std::move(params);
    return;
  }
  if (dr_dc_deps_.enabled) {
    base::WaitableEvent event;
    dr_dc_deps_.thread->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](VizMainImpl* viz_main_impl,
               mojom::FrameSinkManagerParamsPtr params,
               base::WaitableEvent* event) {
              viz_main_impl->CreateFrameSinkManagerInternal(std::move(params));
              event->Signal();
            },
            base::Unretained(this), std::move(params), &event));
    event.Wait();
  } else {
    CreateFrameSinkManagerInternal(std::move(params));
  }
}

void VizMainImpl::CreateFrameSinkManagerInternal(
    mojom::FrameSinkManagerParamsPtr params) {
  DCHECK(gpu_service_);
  auto task_runner = dr_dc_deps_.enabled ? dr_dc_deps_.thread->task_runner()
                                         : gpu_thread_task_runner_;
  DCHECK(task_runner->BelongsToCurrentThread());

  gpu::MailboxManager* mailbox_manager = nullptr;
  scoped_refptr<gl::GLShareGroup> share_group;
  scoped_refptr<gl::GLSurface> offscreen_surface;
  gpu::gles2::ProgramCache* program_cache = nullptr;
  scoped_refptr<gpu::SharedContextState> shared_context_state;

  if (dr_dc_deps_.enabled) {
    mailbox_manager = dr_dc_deps_.mailbox_manager.get();
    share_group = dr_dc_deps_.shared_context_state->share_group();
    offscreen_surface = dr_dc_deps_.shared_context_state->surface();
    shared_context_state = dr_dc_deps_.shared_context_state;
    program_cache = dr_dc_deps_.program_cache.get();
  } else {
    mailbox_manager = gpu_service_->mailbox_manager();
    share_group = gpu_service_->share_group();
    offscreen_surface =
        gpu_service_->gpu_channel_manager()->default_offscreen_surface();
    program_cache = gpu_service_->gpu_channel_manager()->program_cache();
    shared_context_state = gpu_service_->GetContextState();
  }

  gl::GLSurfaceFormat format;
  // If we are running a SW Viz process, we may not have a default offscreen
  // surface.
  if (offscreen_surface) {
    format = offscreen_surface->GetFormat();
  } else {
    DCHECK_EQ(gl::GetGLImplementation(), gl::kGLImplementationDisabled);
  }
  // When the host loses its connection to the viz process, it assumes the
  // process has crashed and tries to reinitialize it. However, it is possible
  // to have lost the connection for other reasons (e.g. deserialization
  // errors) and the viz process is already set up. We cannot recreate
  // FrameSinkManagerImpl, so just do a hard CHECK rather than crashing down the
  // road so that all crash reports caused by this issue look the same and have
  // the same signature. https://crbug.com/928845
  CHECK(!task_executor_);
  task_executor_ = std::make_unique<gpu::GpuInProcessThreadService>(
      task_runner, gpu_service_->scheduler(),
      gpu_service_->sync_point_manager(), mailbox_manager, share_group, format,
      gpu_service_->gpu_feature_info(),
      gpu_service_->gpu_channel_manager()->gpu_preferences(),
      gpu_service_->shared_image_manager(), program_cache,
      shared_context_state);

  viz_compositor_thread_runner_->CreateFrameSinkManager(
      std::move(params), task_executor_.get(), gpu_service_.get());
}

void VizMainImpl::CreateVizDevTools(mojom::VizDevToolsParamsPtr params) {
#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  viz_compositor_thread_runner_->CreateVizDevTools(std::move(params));
#endif
}

void VizMainImpl::ExitProcess() {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  // Close mojom::VizMain bindings first so the browser can't try to reconnect.
  receiver_.reset();

  if (viz_compositor_thread_runner_) {
    // OOP-D requires destroying RootCompositorFrameSinkImpls on the compositor
    // thread while the GPU thread is still running to avoid deadlock. Quit GPU
    // thread TaskRunner after cleanup on compositor thread is finished.
    viz_compositor_thread_runner_->CleanupForShutdown(base::BindOnce(
        &Delegate::QuitMainMessageLoop, base::Unretained(delegate_)));
    if (dr_dc_deps_.enabled)
      dr_dc_deps_.thread->Stop();
  } else {
    delegate_->QuitMainMessageLoop();
  }
}

}  // namespace viz
