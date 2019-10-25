// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_SERVICE_CAST_RENDERER_H_
#define CHROMECAST_MEDIA_SERVICE_CAST_RENDERER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromecast/common/mojom/multiroom.mojom.h"
#include "chromecast/media/base/video_resolution_policy.h"
#include "chromecast/media/cma/backend/cma_backend_factory.h"
#include "media/base/renderer.h"
#include "media/base/waiting.h"
#include "media/mojo/mojom/cast_application_media_info_manager.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace service_manager {
class Connector;
namespace mojom {
class InterfaceProvider;
}  // namespace mojom
}  // namespace service_manager

namespace chromecast {
class TaskRunnerImpl;

namespace media {
class BalancedMediaTaskRunnerFactory;
class CastCdmContext;
class MediaPipelineImpl;
class VideoModeSwitcher;

class CastRenderer : public ::media::Renderer,
                     public VideoResolutionPolicy::Observer {
 public:
  // |connector| provides interfaces for services hosted by ServiceManager.
  // |host_interfaces| provides interfaces tied to RenderFrameHost.
  CastRenderer(CmaBackendFactory* backend_factory,
               const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
               VideoModeSwitcher* video_mode_switcher,
               VideoResolutionPolicy* video_resolution_policy,
               service_manager::Connector* connector,
               service_manager::mojom::InterfaceProvider* host_interfaces);
  ~CastRenderer() final;

  // ::media::Renderer implementation.
  void Initialize(::media::MediaResource* media_resource,
                  ::media::RendererClient* client,
                  const ::media::PipelineStatusCB& init_cb) final;
  void SetCdm(::media::CdmContext* cdm_context,
              const ::media::CdmAttachedCB& cdm_attached_cb) final;
  void Flush(const base::Closure& flush_cb) final;
  void StartPlayingFrom(base::TimeDelta time) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  base::TimeDelta GetMediaTime() final;

  // VideoResolutionPolicy::Observer implementation.
  void OnVideoResolutionPolicyChanged() override;

 private:
  enum Stream { STREAM_AUDIO, STREAM_VIDEO };
  void OnApplicationMediaInfoReceived(
      ::media::MediaResource* media_resource,
      ::media::RendererClient* client,
      const ::media::PipelineStatusCB& init_cb,
      ::media::mojom::CastApplicationMediaInfoPtr application_media_info);
  void OnGetMultiroomInfo(
      ::media::MediaResource* media_resource,
      ::media::RendererClient* client,
      const ::media::PipelineStatusCB& init_cb,
      ::media::mojom::CastApplicationMediaInfoPtr application_media_info,
      chromecast::mojom::MultiroomInfoPtr multiroom_info);
  void OnError(::media::PipelineStatus status);
  void OnEnded(Stream stream);
  void OnStatisticsUpdate(const ::media::PipelineStatistics& stats);
  void OnBufferingStateChange(::media::BufferingState state,
                              ::media::BufferingStateChangeReason reason);
  void OnWaiting(::media::WaitingReason reason);
  void OnVideoNaturalSizeChange(const gfx::Size& size);
  void OnVideoOpacityChange(bool opaque);
  void CheckVideoResolutionPolicy();

  void OnVideoInitializationFinished(const ::media::PipelineStatusCB& init_cb,
                                     ::media::PipelineStatus status);

  CmaBackendFactory* const backend_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  VideoModeSwitcher* video_mode_switcher_;
  VideoResolutionPolicy* video_resolution_policy_;
  service_manager::Connector* connector_;
  service_manager::mojom::InterfaceProvider* host_interfaces_;

  ::media::RendererClient* client_;
  CastCdmContext* cast_cdm_context_;
  scoped_refptr<BalancedMediaTaskRunnerFactory> media_task_runner_factory_;
  std::unique_ptr<TaskRunnerImpl> backend_task_runner_;
  std::unique_ptr<MediaPipelineImpl> pipeline_;
  bool eos_[2];
  gfx::Size video_res_;

  ::media::mojom::CastApplicationMediaInfoManagerPtr
      application_media_info_manager_ptr_;
  mojo::Remote<chromecast::mojom::MultiroomManager> multiroom_manager_;

  base::WeakPtrFactory<CastRenderer> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(CastRenderer);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_SERVICE_CAST_RENDERER_H_
