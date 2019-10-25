// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/fake_assistant_settings_manager_impl.h"

#include <utility>

#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"

namespace chromeos {
namespace assistant {

FakeAssistantSettingsManagerImpl::FakeAssistantSettingsManagerImpl() = default;

FakeAssistantSettingsManagerImpl::~FakeAssistantSettingsManagerImpl() = default;

void FakeAssistantSettingsManagerImpl::GetSettings(
    const std::string& selector,
    GetSettingsCallback callback) {
  // Create a fake response
  assistant::SettingsUi settings_ui;
  settings_ui.mutable_consent_flow_ui()->set_consent_status(
      ConsentFlowUi_ConsentStatus_ALREADY_CONSENTED);
  std::move(callback).Run(settings_ui.SerializeAsString());
}

void FakeAssistantSettingsManagerImpl::UpdateSettings(
    const std::string& update,
    UpdateSettingsCallback callback) {
  std::move(callback).Run(std::string());
}

void FakeAssistantSettingsManagerImpl::StartSpeakerIdEnrollment(
    bool skip_cloud_enrollment,
    mojom::SpeakerIdEnrollmentClientPtr client) {
  client->OnSpeakerIdEnrollmentDone();
}

void FakeAssistantSettingsManagerImpl::StopSpeakerIdEnrollment(
    StopSpeakerIdEnrollmentCallback callback) {
  std::move(callback).Run();
}

void FakeAssistantSettingsManagerImpl::BindRequest(
    mojom::AssistantSettingsManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace assistant
}  // namespace chromeos
