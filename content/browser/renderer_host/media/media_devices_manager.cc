// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <map>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/public/mojom/device_notifications.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/connector.mojom-shared.h"

#if defined(OS_MACOSX)
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "content/browser/browser_main_loop.h"
#include "media/device_monitors/device_monitor_mac.h"
#endif

namespace content {

namespace {

// Resolutions used if the source doesn't support capability enumeration.
struct {
  uint16_t width;
  uint16_t height;
} const kFallbackVideoResolutions[] = {{1920, 1080}, {1280, 720}, {960, 720},
                                       {640, 480},   {640, 360},  {320, 240},
                                       {320, 180}};

// Frame rates for sources with no support for capability enumeration.
const uint16_t kFallbackVideoFrameRates[] = {30, 60};

// Private helper method to generate a string for the log message that lists the
// human readable names of |devices|.
std::string GetLogMessageString(
    blink::MediaDeviceType device_type,
    const blink::WebMediaDeviceInfoArray& device_infos) {
  std::string output_string =
      base::StringPrintf("Getting devices of type %d:\n", device_type);
  if (device_infos.empty())
    return output_string + "No devices found.";
  for (const auto& device_info : device_infos)
    output_string += "  " + device_info.label + "\n";
  return output_string;
}

blink::WebMediaDeviceInfoArray GetFakeAudioDevices(bool is_input) {
  blink::WebMediaDeviceInfoArray result;
  if (is_input) {
    result.emplace_back(media::AudioDeviceDescription::kDefaultDeviceId,
                        "Fake Default Audio Input",
                        "fake_group_audio_input_default");
    result.emplace_back("fake_audio_input_1", "Fake Audio Input 1",
                        "fake_group_audio_input_1");
    result.emplace_back("fake_audio_input_2", "Fake Audio Input 2",
                        "fake_group_audio_input_2");
  } else {
    result.emplace_back(media::AudioDeviceDescription::kDefaultDeviceId,
                        "Fake Default Audio Output",
                        "fake_group_audio_output_default");
    result.emplace_back("fake_audio_output_1", "Fake Audio Output 1",
                        "fake_group_audio_output_1");
    result.emplace_back("fake_audio_output_2", "Fake Audio Output 2",
                        "fake_group_audio_output_2");
  }

  return result;
}

std::string VideoLabelWithoutModelID(const std::string& label) {
  if (label.rfind(")") != label.size() - 1)
    return label;

  auto idx = label.rfind(" (");
  if (idx == std::string::npos)
    return label;

  return label.substr(0, idx - 1);
}

bool LabelHasUSBModel(const std::string& label) {
  return label.size() >= 11 && label[label.size() - 11] == '(' &&
         label[label.size() - 6] == ':' && label[label.size() - 1] == ')';
}

std::string GetUSBModelFromLabel(const std::string& label) {
  DCHECK(LabelHasUSBModel(label));
  return label.substr(label.size() - 10, 9);
}

bool IsRealAudioDeviceID(const std::string& device_id) {
  return !media::AudioDeviceDescription::IsDefaultDevice(device_id) &&
         !media::AudioDeviceDescription::IsCommunicationsDevice(device_id);
}

static bool EqualDeviceAndGroupID(const blink::WebMediaDeviceInfo& lhs,
                                  const blink::WebMediaDeviceInfo& rhs) {
  return lhs == rhs && lhs.group_id == rhs.group_id;
}

void ReplaceInvalidFrameRatesWithFallback(media::VideoCaptureFormats* formats) {
  for (auto& format : *formats) {
    if (format.frame_rate <= 0)
      format.frame_rate = kFallbackVideoFrameRates[0];
  }
}

}  // namespace

std::string GuessVideoGroupID(const blink::WebMediaDeviceInfoArray& audio_infos,
                              const blink::WebMediaDeviceInfo& video_info) {
  const std::string video_label = VideoLabelWithoutModelID(video_info.label);

  // If |video_label| is very small, do not guess in order to avoid false
  // positives.
  if (video_label.size() <= 3)
    return video_info.device_id;

  base::RepeatingCallback<bool(const blink::WebMediaDeviceInfo&)>
      video_label_is_included_in_audio_label = base::BindRepeating(
          [](const std::string& video_label,
             const blink::WebMediaDeviceInfo& audio_info) {
            return audio_info.label.find(video_label) != std::string::npos;
          },
          std::cref(video_label));

  const bool video_has_usb_model = LabelHasUSBModel(video_info.label);
  std::string video_usb_model = video_has_usb_model
                                    ? GetUSBModelFromLabel(video_info.label)
                                    : std::string();
  base::RepeatingCallback<bool(const blink::WebMediaDeviceInfo&)>
      usb_model_matches = base::BindRepeating(
          [](bool video_has_usb_model, const std::string& video_usb_model,
             const blink::WebMediaDeviceInfo& audio_info) {
            return video_has_usb_model && LabelHasUSBModel(audio_info.label)
                       ? video_usb_model ==
                             GetUSBModelFromLabel(audio_info.label)
                       : false;
          },
          video_has_usb_model, std::cref(video_usb_model));

  for (auto* callback :
       {&video_label_is_included_in_audio_label, &usb_model_matches}) {
    // The label for the default and communication audio devices may contain the
    // same label as the real devices, so they should be ignored when trying to
    // find unique matches.
    auto real_device_matches =
        [callback](const blink::WebMediaDeviceInfo& audio_info) {
          return IsRealAudioDeviceID(audio_info.device_id) &&
                 (*callback).Run(audio_info);
        };
    auto it_first = std::find_if(audio_infos.begin(), audio_infos.end(),
                                 real_device_matches);
    if (it_first == audio_infos.end())
      continue;

    auto it = it_first;
    bool duplicate_found = false;
    while ((it = std::find_if(it + 1, audio_infos.end(),
                              real_device_matches)) != audio_infos.end()) {
      // If there is more than one match, it is impossible to know which group
      // ID is the correct one. This may occur if multiple devices of the same
      // model are installed.
      if (it->group_id != it_first->group_id) {
        duplicate_found = true;
        break;
      }
    }

    if (!duplicate_found)
      return it_first->group_id;
  }

  return video_info.device_id;
}

struct MediaDevicesManager::EnumerationRequest {
  EnumerationRequest(const BoolDeviceTypes& requested_types,
                     EnumerationCallback callback)
      : callback(std::move(callback)) {
    requested = requested_types;
    has_seen_result.fill(false);
  }

  BoolDeviceTypes requested;
  BoolDeviceTypes has_seen_result;
  EnumerationCallback callback;
};

// This class helps manage the consistency of cached enumeration results.
// It uses a sequence number for each invalidation and enumeration.
// A cache is considered valid if the sequence number for the last enumeration
// is greater than the sequence number for the last invalidation.
// The advantage of using invalidations over directly issuing enumerations upon
// each system notification is that some platforms issue multiple notifications
// on each device change. The cost of performing multiple redundant
// invalidations is significantly lower than the cost of issuing multiple
// redundant enumerations.
class MediaDevicesManager::CacheInfo {
 public:
  CacheInfo()
      : current_event_sequence_(0),
        seq_last_update_(0),
        seq_last_invalidation_(0),
        is_update_ongoing_(false) {}

  void InvalidateCache() {
    DCHECK(thread_checker_.CalledOnValidThread());
    seq_last_invalidation_ = NewEventSequence();
  }

  bool IsLastUpdateValid() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return seq_last_update_ > seq_last_invalidation_ && !is_update_ongoing_;
  }

  void UpdateStarted() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!is_update_ongoing_);
    seq_last_update_ = NewEventSequence();
    is_update_ongoing_ = true;
  }

  void UpdateCompleted() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(is_update_ongoing_);
    is_update_ongoing_ = false;
  }

  bool is_update_ongoing() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return is_update_ongoing_;
  }

 private:
  int64_t NewEventSequence() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return ++current_event_sequence_;
  }

  int64_t current_event_sequence_;
  int64_t seq_last_update_;
  int64_t seq_last_invalidation_;
  bool is_update_ongoing_;
  base::ThreadChecker thread_checker_;
};

MediaDevicesManager::SubscriptionRequest::SubscriptionRequest(
    int render_process_id,
    int render_frame_id,
    const BoolDeviceTypes& subscribe_types,
    mojo::Remote<blink::mojom::MediaDevicesListener> listener)
    : render_process_id(render_process_id),
      render_frame_id(render_frame_id),
      subscribe_types(subscribe_types),
      listener_(std::move(listener)) {}

MediaDevicesManager::SubscriptionRequest::SubscriptionRequest(
    SubscriptionRequest&&) = default;

MediaDevicesManager::SubscriptionRequest::~SubscriptionRequest() = default;

MediaDevicesManager::SubscriptionRequest&
MediaDevicesManager::SubscriptionRequest::operator=(SubscriptionRequest&&) =
    default;

class MediaDevicesManager::AudioServiceDeviceListener
    : public audio::mojom::DeviceListener {
 public:
  explicit AudioServiceDeviceListener(service_manager::Connector* connector) {
    TryConnectToService(connector);
  }
  ~AudioServiceDeviceListener() override = default;

  void DevicesChanged() override {
    auto* system_monitor = base::SystemMonitor::Get();
    if (system_monitor)
      system_monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  }

 private:
  void TryConnectToService(service_manager::Connector* connector) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Check if the service manager is managing the audio service.
    //
    // TODO: Is this necessary? We should know at build how this will respond.
    connector->QueryService(
        audio::mojom::kServiceName,
        base::BindOnce(&AudioServiceDeviceListener::ServiceQueried,
                       weak_factory_.GetWeakPtr(), connector));
  }

  void ServiceQueried(service_manager::Connector* connector,
                      service_manager::mojom::ServiceInfoPtr info) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Do not connect if the service manager is not managing the audio service.
    if (!info) {
      LOG(WARNING) << "Audio service not available.";
      return;
    }
    DoConnectToService(connector);
  }

  void DoConnectToService(service_manager::Connector* connector) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!mojo_audio_device_notifier_);
    DCHECK(!receiver_.is_bound());
    connector->Connect(
        audio::mojom::kServiceName,
        mojo_audio_device_notifier_.BindNewPipeAndPassReceiver());
    mojo_audio_device_notifier_.set_disconnect_handler(base::BindOnce(
        &MediaDevicesManager::AudioServiceDeviceListener::OnConnectionError,
        weak_factory_.GetWeakPtr(), connector));
    mojo_audio_device_notifier_->RegisterListener(
        receiver_.BindNewPipeAndPassRemote());
  }

  void OnConnectionError(service_manager::Connector* connector) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    mojo_audio_device_notifier_.reset();
    receiver_.reset();

    // Resetting the error handler in a posted task since doing it synchronously
    // results in a browser crash. See https://crbug.com/845142.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioServiceDeviceListener::TryConnectToService,
                       weak_factory_.GetWeakPtr(), connector));
  }

  mojo::Receiver<audio::mojom::DeviceListener> receiver_{this};
  mojo::Remote<audio::mojom::DeviceNotifier> mojo_audio_device_notifier_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AudioServiceDeviceListener> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioServiceDeviceListener);
};

MediaDevicesManager::MediaDevicesManager(
    media::AudioSystem* audio_system,
    const scoped_refptr<VideoCaptureManager>& video_capture_manager,
    StopRemovedInputDeviceCallback stop_removed_input_device_cb,
    UIInputDeviceChangeCallback ui_input_device_change_cb)
    : use_fake_devices_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)),
      audio_system_(audio_system),
      video_capture_manager_(video_capture_manager),
      stop_removed_input_device_cb_(std::move(stop_removed_input_device_cb)),
      ui_input_device_change_cb_(std::move(ui_input_device_change_cb)),
      permission_checker_(std::make_unique<MediaDevicesPermissionChecker>()),
      cache_infos_(blink::NUM_MEDIA_DEVICE_TYPES),
      monitoring_started_(false),
      salt_and_origin_callback_(
          base::BindRepeating(&GetMediaDeviceSaltAndOrigin)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(audio_system_);
  DCHECK(video_capture_manager_.get());
  DCHECK(!stop_removed_input_device_cb_.is_null());
  DCHECK(!ui_input_device_change_cb_.is_null());
  cache_policies_.fill(CachePolicy::NO_CACHE);
  has_seen_result_.fill(false);
}

MediaDevicesManager::~MediaDevicesManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MediaDevicesManager::EnumerateDevices(
    const BoolDeviceTypes& requested_types,
    EnumerationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StartMonitoring();

  requests_.emplace_back(requested_types, std::move(callback));
  bool all_results_cached = true;
  for (size_t i = 0; i < blink::NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (requested_types[i] && cache_policies_[i] == CachePolicy::NO_CACHE) {
      all_results_cached = false;
      DoEnumerateDevices(static_cast<blink::MediaDeviceType>(i));
    }
  }

  if (all_results_cached)
    ProcessRequests();
}

void MediaDevicesManager::EnumerateDevices(
    int render_process_id,
    int render_frame_id,
    const BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_video_input_capabilities &&
             requested_types[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT] ||
         !request_video_input_capabilities);
  DCHECK(request_audio_input_capabilities &&
             requested_types[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] ||
         !request_audio_input_capabilities);

  base::PostTaskAndReplyWithResult(
      base::CreateSingleThreadTaskRunner({BrowserThread::UI}).get(), FROM_HERE,
      base::BindOnce(salt_and_origin_callback_, render_process_id,
                     render_frame_id),
      base::BindOnce(&MediaDevicesManager::CheckPermissionsForEnumerateDevices,
                     weak_factory_.GetWeakPtr(), render_process_id,
                     render_frame_id, requested_types,
                     request_video_input_capabilities,
                     request_audio_input_capabilities, std::move(callback)));
}

uint32_t MediaDevicesManager::SubscribeDeviceChangeNotifications(
    int render_process_id,
    int render_frame_id,
    const BoolDeviceTypes& subscribe_types,
    mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StartMonitoring();
  uint32_t subscription_id = ++last_subscription_id_;
  mojo::Remote<blink::mojom::MediaDevicesListener> media_devices_listener;
  media_devices_listener.Bind(std::move(listener));
  media_devices_listener.set_disconnect_handler(
      base::BindOnce(&MediaDevicesManager::UnsubscribeDeviceChangeNotifications,
                     weak_factory_.GetWeakPtr(), subscription_id));
  subscriptions_.emplace(
      subscription_id,
      SubscriptionRequest(render_process_id, render_frame_id, subscribe_types,
                          std::move(media_devices_listener)));

  return subscription_id;
}

void MediaDevicesManager::UnsubscribeDeviceChangeNotifications(
    uint32_t subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  subscriptions_.erase(subscription_id);
}

void MediaDevicesManager::SetCachePolicy(blink::MediaDeviceType type,
                                         CachePolicy policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  if (cache_policies_[type] == policy)
    return;

  cache_policies_[type] = policy;
  // If the new policy is SYSTEM_MONITOR, issue an enumeration to populate the
  // cache.
  if (policy == CachePolicy::SYSTEM_MONITOR) {
    cache_infos_[type].InvalidateCache();
    DoEnumerateDevices(type);
  }
}

void MediaDevicesManager::StartMonitoring() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (monitoring_started_)
    return;

  if (!base::SystemMonitor::Get())
    return;

#if defined(OS_MACOSX)
  if (!base::FeatureList::IsEnabled(features::kDeviceMonitorMac))
    return;
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
  if (base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess)) {
    DCHECK(!audio_service_device_listener_);
    if (!connector_) {
      auto* connector = ServiceManagerContext::GetConnectorForIOThread();
      // |connector| can be null on unit tests.
      if (!connector)
        return;

      connector_ = connector->Clone();
    }

    audio_service_device_listener_ =
        std::make_unique<AudioServiceDeviceListener>(connector_.get());
  }
#endif
  monitoring_started_ = true;
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);

  if (base::FeatureList::IsEnabled(features::kMediaDevicesSystemMonitorCache)) {
    for (size_t i = 0; i < blink::NUM_MEDIA_DEVICE_TYPES; ++i) {
      DCHECK(cache_policies_[i] != CachePolicy::SYSTEM_MONITOR);
      SetCachePolicy(static_cast<blink::MediaDeviceType>(i),
                     CachePolicy::SYSTEM_MONITOR);
    }
  }

#if defined(OS_MACOSX)
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&MediaDevicesManager::StartMonitoringOnUIThread,
                                base::Unretained(this)));
#endif
}

#if defined(OS_MACOSX)
void MediaDevicesManager::StartMonitoringOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserMainLoop* browser_main_loop = content::BrowserMainLoop::GetInstance();
  if (!browser_main_loop)
    return;
  browser_main_loop->device_monitor_mac()->StartMonitoring();
}
#endif

void MediaDevicesManager::StopMonitoring() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!monitoring_started_)
    return;
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
  audio_service_device_listener_.reset();
  monitoring_started_ = false;
  for (size_t i = 0; i < blink::NUM_MEDIA_DEVICE_TYPES; ++i)
    SetCachePolicy(static_cast<blink::MediaDeviceType>(i),
                   CachePolicy::NO_CACHE);
}

bool MediaDevicesManager::IsMonitoringStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return monitoring_started_;
}

void MediaDevicesManager::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (device_type) {
    case base::SystemMonitor::DEVTYPE_AUDIO:
      HandleDevicesChanged(blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT);
      HandleDevicesChanged(blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT);
      break;
    case base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE:
      HandleDevicesChanged(blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT);
      break;
    default:
      break;  // Uninteresting device change.
  }
}

media::VideoCaptureFormats MediaDevicesManager::GetVideoInputFormats(
    const std::string& device_id,
    bool try_in_use_first) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media::VideoCaptureFormats formats;

  if (try_in_use_first) {
    base::Optional<media::VideoCaptureFormat> format =
        video_capture_manager_->GetDeviceFormatInUse(
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, device_id);
    if (format.has_value()) {
      formats.push_back(format.value());
      ReplaceInvalidFrameRatesWithFallback(&formats);
      return formats;
    }
  }

  video_capture_manager_->GetDeviceSupportedFormats(device_id, &formats);
  ReplaceInvalidFrameRatesWithFallback(&formats);
  // Remove formats that have zero resolution.
  base::EraseIf(formats, [](const media::VideoCaptureFormat& format) {
    return format.frame_size.GetArea() <= 0;
  });

  // If the device does not report any valid format, use a fallback list of
  // standard formats.
  if (formats.empty()) {
    for (const auto& resolution : kFallbackVideoResolutions) {
      for (const auto frame_rate : kFallbackVideoFrameRates) {
        formats.push_back(media::VideoCaptureFormat(
            gfx::Size(resolution.width, resolution.height), frame_rate,
            media::PIXEL_FORMAT_I420));
      }
    }
  }

  return formats;
}

blink::WebMediaDeviceInfoArray MediaDevicesManager::GetCachedDeviceInfo(
    blink::MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return current_snapshot_[type];
}

MediaDevicesPermissionChecker*
MediaDevicesManager::media_devices_permission_checker() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return permission_checker_.get();
}

void MediaDevicesManager::SetPermissionChecker(
    std::unique_ptr<MediaDevicesPermissionChecker> permission_checker) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(permission_checker);
  permission_checker_ = std::move(permission_checker);
}

void MediaDevicesManager::CheckPermissionsForEnumerateDevices(
    int render_process_id,
    int render_frame_id,
    const BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback callback,
    MediaDeviceSaltAndOrigin salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  permission_checker_->CheckPermissions(
      requested_types, render_process_id, render_frame_id,
      base::BindOnce(&MediaDevicesManager::OnPermissionsCheckDone,
                     weak_factory_.GetWeakPtr(), requested_types,
                     request_video_input_capabilities,
                     request_audio_input_capabilities, std::move(callback),
                     std::move(salt_and_origin)));
}

void MediaDevicesManager::OnPermissionsCheckDone(
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback callback,
    MediaDeviceSaltAndOrigin salt_and_origin,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // The video-capture subsystem currently does not support group IDs.
  // If video input devices are requested, also request audio input devices in
  // order to be able to use an heuristic that guesses group IDs for video
  // devices by finding matches in audio input devices.
  // TODO(crbug.com/627793): Remove |internal_requested_types| and use
  // |requested_types| directly when video capture supports group IDs.
  BoolDeviceTypes internal_requested_types;
  internal_requested_types[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] =
      requested_types[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] ||
      requested_types[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT];
  internal_requested_types[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT] =
      requested_types[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT];
  internal_requested_types[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] =
      requested_types[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT];

  EnumerateDevices(
      internal_requested_types,
      base::BindOnce(&MediaDevicesManager::OnDevicesEnumerated,
                     weak_factory_.GetWeakPtr(), requested_types,
                     request_video_input_capabilities,
                     request_audio_input_capabilities, std::move(callback),
                     std::move(salt_and_origin), has_permissions));
}

void MediaDevicesManager::OnDevicesEnumerated(
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback callback,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const bool video_input_capabilities_requested =
      has_permissions[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT] &&
      request_video_input_capabilities;
  const bool audio_input_capabilities_requested =
      has_permissions[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] &&
      request_audio_input_capabilities;

  std::vector<blink::WebMediaDeviceInfoArray> translation(
      blink::NUM_MEDIA_DEVICE_TYPES);
  for (size_t i = 0; i < blink::NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (!requested_types[i])
      continue;

    for (const auto& device_info : enumeration[i]) {
      translation[i].push_back(TranslateMediaDeviceInfo(
          has_permissions[i], salt_and_origin, device_info));
    }
  }

  GetAudioInputCapabilities(video_input_capabilities_requested,
                            audio_input_capabilities_requested,
                            std::move(callback), enumeration, translation);
}

void MediaDevicesManager::GetAudioInputCapabilities(
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback callback,
    const MediaDeviceEnumeration& raw_enumeration_results,
    const std::vector<blink::WebMediaDeviceInfoArray>&
        hashed_enumeration_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  EnumerationState state;
  size_t state_id = next_enumeration_state_id_++;
  state.video_input_capabilities_requested = request_video_input_capabilities;
  state.audio_input_capabilities_requested = request_audio_input_capabilities;
  state.completion_cb = std::move(callback);
  state.raw_enumeration_results = std::move(raw_enumeration_results);
  state.hashed_enumeration_results = std::move(hashed_enumeration_results);
  state.num_pending_audio_input_capabilities =
      hashed_enumeration_results[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT].size();

  if (!state.audio_input_capabilities_requested ||
      state.num_pending_audio_input_capabilities == 0) {
    FinalizeDevicesEnumerated(std::move(state));
    return;
  }

  enumeration_states_[state_id] = std::move(state);
  DCHECK_EQ(
      raw_enumeration_results[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT].size(),
      hashed_enumeration_results[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT].size());
  std::size_t num_audio_input_devices =
      raw_enumeration_results[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT].size();
  for (std::size_t i = 0; i < num_audio_input_devices; i++) {
    auto raw_device_info =
        raw_enumeration_results[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT][i];
    auto hashed_device_info =
        hashed_enumeration_results[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT][i];

    AudioInputDeviceCapabilitiesPtr capabilities =
        blink::mojom::AudioInputDeviceCapabilities::New();
    capabilities->device_id = hashed_device_info.device_id;
    capabilities->parameters =
        media::AudioParameters::UnavailableDeviceParams();
    enumeration_states_[state_id].audio_capabilities.push_back(
        std::move(capabilities));
    size_t capabilities_index =
        enumeration_states_[state_id].audio_capabilities.size() - 1;
    if (use_fake_devices_) {
      base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&MediaDevicesManager::GotAudioInputCapabilities,
                         weak_factory_.GetWeakPtr(), state_id,
                         capabilities_index,
                         media::AudioParameters::UnavailableDeviceParams()));
    } else {
      audio_system_->GetInputStreamParameters(
          raw_device_info.device_id,
          base::BindOnce(&MediaDevicesManager::GotAudioInputCapabilities,
                         weak_factory_.GetWeakPtr(), state_id,
                         capabilities_index));
    }
  }
}

void MediaDevicesManager::GotAudioInputCapabilities(
    size_t state_id,
    size_t capabilities_index,
    const base::Optional<media::AudioParameters>& parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::Contains(enumeration_states_, state_id));

  auto& enumeration_state = enumeration_states_[state_id];
  DCHECK_GT(enumeration_state.num_pending_audio_input_capabilities, 0);

  AudioInputDeviceCapabilitiesPtr& capabilities =
      enumeration_state.audio_capabilities[capabilities_index];
  if (parameters) {
    capabilities->parameters = *parameters;
    // Data from the |parameters| field is duplicated in the |channels|,
    // |sample_rate| and |latency| fields due to the lack of availability
    // of the media::AudioParameters native mojo mapping in blink.
    // TODO(crbug.com/787252): Remove redundant fields when |parameters|
    // is accessible from Blink.
    capabilities->is_valid = parameters->IsValid();
    capabilities->channels = parameters->channels();
    capabilities->sample_rate = parameters->sample_rate();
    capabilities->latency = parameters->GetBufferDuration();
  }
  DCHECK(capabilities->parameters.IsValid());

  if (--enumeration_state.num_pending_audio_input_capabilities == 0) {
    FinalizeDevicesEnumerated(std::move(enumeration_state));
    enumeration_states_.erase(state_id);
  }
}

void MediaDevicesManager::FinalizeDevicesEnumerated(
    EnumerationState enumeration_state) {
  std::move(enumeration_state.completion_cb)
      .Run(std::move(enumeration_state.hashed_enumeration_results),
           enumeration_state.video_input_capabilities_requested
               ? ComputeVideoInputCapabilities(
                     enumeration_state.raw_enumeration_results
                         [blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT],
                     enumeration_state.hashed_enumeration_results
                         [blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT])
               : std::vector<VideoInputDeviceCapabilitiesPtr>(),
           std::move(enumeration_state.audio_capabilities));
}

std::vector<VideoInputDeviceCapabilitiesPtr>
MediaDevicesManager::ComputeVideoInputCapabilities(
    const blink::WebMediaDeviceInfoArray& raw_device_infos,
    const blink::WebMediaDeviceInfoArray& translated_device_infos) {
  DCHECK_EQ(raw_device_infos.size(), translated_device_infos.size());
  std::vector<VideoInputDeviceCapabilitiesPtr> video_input_capabilities;
  for (size_t i = 0; i < raw_device_infos.size(); ++i) {
    VideoInputDeviceCapabilitiesPtr capabilities =
        blink::mojom::VideoInputDeviceCapabilities::New();
    capabilities->device_id = translated_device_infos[i].device_id;
    capabilities->formats = GetVideoInputFormats(raw_device_infos[i].device_id,
                                                 false /* try_in_use_first */);
    capabilities->facing_mode = translated_device_infos[i].video_facing;
    video_input_capabilities.push_back(std::move(capabilities));
  }
  return video_input_capabilities;
}

void MediaDevicesManager::DoEnumerateDevices(blink::MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  CacheInfo& cache_info = cache_infos_[type];
  if (cache_info.is_update_ongoing())
    return;

  cache_info.UpdateStarted();
  switch (type) {
    case blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      EnumerateAudioDevices(true /* is_input */);
      break;
    case blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      video_capture_manager_->EnumerateDevices(
          base::BindOnce(&MediaDevicesManager::VideoInputDevicesEnumerated,
                         weak_factory_.GetWeakPtr()));
      break;
    case blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      EnumerateAudioDevices(false /* is_input */);
      break;
    default:
      NOTREACHED();
  }
}

void MediaDevicesManager::EnumerateAudioDevices(bool is_input) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blink::MediaDeviceType type = is_input
                                    ? blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT
                                    : blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT;
  if (use_fake_devices_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&MediaDevicesManager::DevicesEnumerated,
                                  weak_factory_.GetWeakPtr(), type,
                                  GetFakeAudioDevices(is_input)));
    return;
  }

  audio_system_->GetDeviceDescriptions(
      is_input, base::BindOnce(&MediaDevicesManager::AudioDevicesEnumerated,
                               weak_factory_.GetWeakPtr(), type));
}

void MediaDevicesManager::VideoInputDevicesEnumerated(
    const media::VideoCaptureDeviceDescriptors& descriptors) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blink::WebMediaDeviceInfoArray snapshot;
  for (const auto& descriptor : descriptors) {
    snapshot.emplace_back(descriptor);
  }
  DevicesEnumerated(blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT, snapshot);
}

void MediaDevicesManager::AudioDevicesEnumerated(
    blink::MediaDeviceType type,
    media::AudioDeviceDescriptions device_descriptions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  blink::WebMediaDeviceInfoArray snapshot;
  for (const media::AudioDeviceDescription& description : device_descriptions) {
    snapshot.emplace_back(description.unique_id, description.device_name,
                          description.group_id,
                          media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE);
  }
  DevicesEnumerated(type, snapshot);
}

void MediaDevicesManager::DevicesEnumerated(
    blink::MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  UpdateSnapshot(type, snapshot);
  cache_infos_[type].UpdateCompleted();
  has_seen_result_[type] = true;

  std::string log_message =
      "New device enumeration result:\n" + GetLogMessageString(type, snapshot);
  MediaStreamManager::SendMessageToNativeLog(log_message);

  if (cache_policies_[type] == CachePolicy::NO_CACHE) {
    for (auto& request : requests_)
      request.has_seen_result[type] = true;
  }

  // Note that IsLastUpdateValid is always true when policy is NO_CACHE.
  if (cache_infos_[type].IsLastUpdateValid()) {
    ProcessRequests();
  } else {
    DoEnumerateDevices(type);
  }
}

void MediaDevicesManager::UpdateSnapshot(
    blink::MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& new_snapshot,
    bool ignore_group_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));

  bool need_update_device_change_subscribers = false;
  blink::WebMediaDeviceInfoArray& old_snapshot = current_snapshot_[type];

  if (type == blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT ||
      type == blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT) {
    MaybeStopRemovedInputDevices(type, new_snapshot);
  }

  // Update the cached snapshot and send notifications only if the device list
  // has changed.
  if (old_snapshot.size() != new_snapshot.size() ||
      !std::equal(
          new_snapshot.begin(), new_snapshot.end(), old_snapshot.begin(),
          ignore_group_id
              ? [](const blink::WebMediaDeviceInfo& lhs,
                   const blink::WebMediaDeviceInfo& rhs) { return lhs == rhs; }
              : EqualDeviceAndGroupID)) {
    // Prevent sending notifications until group IDs are updated using
    // a heuristic in ProcessRequests().
    // TODO(crbug.com/627793): Remove |is_video_with_group_ids| and the
    // corresponding checks when the video-capture subsystem supports
    // group IDs.
    bool is_video_with_good_group_ids =
        type == blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT &&
        (new_snapshot.size() == 0 || !new_snapshot[0].group_id.empty());
    if (type == blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT ||
        is_video_with_good_group_ids)
      ui_input_device_change_cb_.Run(type, new_snapshot);

    // Do not notify device-change subscribers after the first enumeration
    // result, since it is not due to an actual device change.
    need_update_device_change_subscribers =
        has_seen_result_[type] &&
        (old_snapshot.size() != 0 || new_snapshot.size() != 0) &&
        (type != blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT ||
         is_video_with_good_group_ids);
    current_snapshot_[type] = new_snapshot;
  }

  if (need_update_device_change_subscribers)
    NotifyDeviceChangeSubscribers(type, new_snapshot);
}

void MediaDevicesManager::ProcessRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Populate the group ID field for video devices using a heuristic that looks
  // for device coincidences with audio input devices.
  // TODO(crbug.com/627793): Remove this once the video-capture subsystem
  // supports group IDs.
  if (has_seen_result_[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT]) {
    blink::WebMediaDeviceInfoArray video_devices =
        current_snapshot_[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT];
    for (auto& video_device_info : video_devices) {
      video_device_info.group_id = GuessVideoGroupID(
          current_snapshot_[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT],
          video_device_info);
    }
    UpdateSnapshot(blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT, video_devices,
                   false /* ignore_group_id */);
  }

  base::EraseIf(requests_, [this](EnumerationRequest& request) {
    if (IsEnumerationRequestReady(request)) {
      std::move(request.callback).Run(current_snapshot_);
      return true;
    }
    return false;
  });
}

bool MediaDevicesManager::IsEnumerationRequestReady(
    const EnumerationRequest& request_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool is_ready = true;
  for (size_t i = 0; i < blink::NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (!request_info.requested[i])
      continue;
    switch (cache_policies_[i]) {
      case CachePolicy::SYSTEM_MONITOR:
        if (!cache_infos_[i].IsLastUpdateValid())
          is_ready = false;
        break;
      case CachePolicy::NO_CACHE:
        if (!request_info.has_seen_result[i])
          is_ready = false;
        break;
      default:
        NOTREACHED();
    }
  }
  return is_ready;
}

void MediaDevicesManager::HandleDevicesChanged(blink::MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  cache_infos_[type].InvalidateCache();
  DoEnumerateDevices(type);
}

void MediaDevicesManager::MaybeStopRemovedInputDevices(
    blink::MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& new_snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT ||
         type == blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT);

  std::vector<blink::WebMediaDeviceInfo> removed_audio_devices;
  for (const auto& old_device_info : current_snapshot_[type]) {
    auto it =
        std::find_if(new_snapshot.begin(), new_snapshot.end(),
                     [&old_device_info](const blink::WebMediaDeviceInfo& info) {
                       return info.device_id == old_device_info.device_id;
                     });

    // If a device was removed, notify the MediaStreamManager to stop all
    // streams using that device.
    if (it == new_snapshot.end()) {
      stop_removed_input_device_cb_.Run(type, old_device_info);

      if (type == blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT)
        removed_audio_devices.push_back(old_device_info);
    }
  }

  // "default" and "communications" audio devices that have been removed,
  // require an extra notification. In fact, such audio devices have associated
  // virtual audio devices in the snapshot with the special "default" or
  // "communications" IDs. The code below implements an heuristic, such that to
  // identify if an audio device was default, it checks whether the old
  // snapshot contained an audio device with the same group ID and device ID
  // matching either "default" or "communications".
  // NOTE: ChromeOS is able to seamlessly redirect streams to the new default
  // device, hence the event should not be triggered.
#if !defined(OS_CHROMEOS)
  for (const auto& removed_audio_device : removed_audio_devices) {
    for (const auto& old_device_info : current_snapshot_[type]) {
      if (removed_audio_device.group_id == old_device_info.group_id &&
          (old_device_info.device_id ==
               media::AudioDeviceDescription::kDefaultDeviceId ||
           old_device_info.device_id ==
               media::AudioDeviceDescription::kCommunicationsDeviceId)) {
        stop_removed_input_device_cb_.Run(type, old_device_info);
      }
    }
  }
#endif  // !defined(OS_CHROMEOS)
}

void MediaDevicesManager::NotifyDeviceChangeSubscribers(
    blink::MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));

  for (auto& subscription : subscriptions_) {
    const SubscriptionRequest& request = subscription.second;
    if (request.subscribe_types[type]) {
      base::PostTaskAndReplyWithResult(
          base::CreateSingleThreadTaskRunner({BrowserThread::UI}).get(),
          FROM_HERE,
          base::BindOnce(salt_and_origin_callback_, request.render_process_id,
                         request.render_frame_id),
          base::BindOnce(&MediaDevicesManager::CheckPermissionForDeviceChange,
                         weak_factory_.GetWeakPtr(), subscription.first,
                         request.render_process_id, request.render_frame_id,
                         type, snapshot));
    }
  }
}

void MediaDevicesManager::CheckPermissionForDeviceChange(
    uint32_t subscription_id,
    int render_process_id,
    int render_frame_id,
    blink::MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& device_infos,
    MediaDeviceSaltAndOrigin salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  permission_checker_->CheckPermission(
      type, render_process_id, render_frame_id,
      base::BindOnce(&MediaDevicesManager::NotifyDeviceChange,
                     weak_factory_.GetWeakPtr(), subscription_id, type,
                     device_infos, std::move(salt_and_origin)));
}

void MediaDevicesManager::NotifyDeviceChange(
    uint32_t subscription_id,
    blink::MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& device_infos,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    bool has_permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  auto it = subscriptions_.find(subscription_id);
  if (it == subscriptions_.end())
    return;

  const SubscriptionRequest& request = it->second;
  request.listener_->OnDevicesChanged(
      type, TranslateMediaDeviceInfoArray(has_permission, salt_and_origin,
                                          device_infos));
}

MediaDevicesManager::EnumerationState::EnumerationState() = default;
MediaDevicesManager::EnumerationState::EnumerationState(
    EnumerationState&& other) = default;
MediaDevicesManager::EnumerationState::~EnumerationState() = default;
MediaDevicesManager::EnumerationState& MediaDevicesManager::EnumerationState::
operator=(EnumerationState&& other) = default;

}  // namespace content
