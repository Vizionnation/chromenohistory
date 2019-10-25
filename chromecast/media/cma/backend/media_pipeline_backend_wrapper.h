// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_WRAPPER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_WRAPPER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chromecast/media/base/media_resource_tracker.h"
#include "chromecast/media/cma/backend/cma_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

enum class AudioContentType;
class AudioDecoderWrapper;
class VideoDecoderWrapper;
class MediaPipelineBackend;
class MediaPipelineBackendManager;
class DecoderCreatorCmaBackend;

class MediaPipelineBackendWrapper : public CmaBackend {
 public:
  MediaPipelineBackendWrapper(const media::MediaPipelineDeviceParams& params,
                              MediaPipelineBackendManager* backend_manager,
                              MediaResourceTracker* media_resource_tracker);
  ~MediaPipelineBackendWrapper() override;

  // After revocation, this class releases the media resource on the device,
  // so the next MediaPipelineBackend can be created for the next application.
  // See b/69180616.
  void Revoke();

  // CmaBackend implementation:
  AudioDecoder* CreateAudioDecoder() override;
  VideoDecoder* CreateVideoDecoder() override;
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  int64_t GetCurrentPts() override;
  bool SetPlaybackRate(float rate) override;
  void LogicalPause() override;
  void LogicalResume() override;

 private:
  std::unique_ptr<AudioDecoderWrapper> audio_decoder_;
  std::unique_ptr<VideoDecoderWrapper> video_decoder_;

  bool revoked_;
  std::unique_ptr<DecoderCreatorCmaBackend> backend_;
  MediaPipelineBackendManager* const backend_manager_;
  const AudioContentType content_type_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendWrapper);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_WRAPPER_H_
