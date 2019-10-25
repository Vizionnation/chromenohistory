// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_software_feature_manager.h"

namespace chromeos {

namespace device_sync {

FakeSoftwareFeatureManager::SetSoftwareFeatureStateArgs::
    SetSoftwareFeatureStateArgs(
        const std::string& public_key,
        multidevice::SoftwareFeature software_feature,
        bool enabled,
        const base::Closure& success_callback,
        const base::Callback<void(NetworkRequestError)>& error_callback,
        bool is_exclusive)
    : public_key(public_key),
      software_feature(software_feature),
      enabled(enabled),
      success_callback(success_callback),
      error_callback(error_callback),
      is_exclusive(is_exclusive) {}

FakeSoftwareFeatureManager::SetSoftwareFeatureStateArgs::
    ~SetSoftwareFeatureStateArgs() = default;

FakeSoftwareFeatureManager::FindEligibleDevicesArgs::FindEligibleDevicesArgs(
    multidevice::SoftwareFeature software_feature,
    const base::Callback<void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                              const std::vector<cryptauth::IneligibleDevice>&)>&
        success_callback,
    const base::Callback<void(NetworkRequestError)>& error_callback)
    : software_feature(software_feature),
      success_callback(success_callback),
      error_callback(error_callback) {}

FakeSoftwareFeatureManager::FindEligibleDevicesArgs::
    ~FindEligibleDevicesArgs() = default;

FakeSoftwareFeatureManager::FakeSoftwareFeatureManager() = default;

FakeSoftwareFeatureManager::~FakeSoftwareFeatureManager() = default;

void FakeSoftwareFeatureManager::SetSoftwareFeatureState(
    const std::string& public_key,
    multidevice::SoftwareFeature software_feature,
    bool enabled,
    const base::Closure& success_callback,
    const base::Callback<void(NetworkRequestError)>& error_callback,
    bool is_exclusive) {
  set_software_feature_state_calls_.emplace_back(
      std::make_unique<SetSoftwareFeatureStateArgs>(
          public_key, software_feature, enabled, success_callback,
          error_callback, is_exclusive));

  if (delegate_)
    delegate_->OnSetSoftwareFeatureStateCalled();
}

void FakeSoftwareFeatureManager::FindEligibleDevices(
    multidevice::SoftwareFeature software_feature,
    const base::Callback<void(const std::vector<cryptauth::ExternalDeviceInfo>&,
                              const std::vector<cryptauth::IneligibleDevice>&)>&
        success_callback,
    const base::Callback<void(NetworkRequestError)>& error_callback) {
  find_eligible_multidevice_host_calls_.emplace_back(
      std::make_unique<FindEligibleDevicesArgs>(
          software_feature, success_callback, error_callback));

  if (delegate_)
    delegate_->OnFindEligibleDevicesCalled();
}

}  // namespace device_sync

}  // namespace chromeos
