// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/services/device_sync/device_sync_base.h"

namespace chromeos {

namespace device_sync {

DeviceSyncBase::DeviceSyncBase() {
  bindings_.set_connection_error_handler(base::BindRepeating(
      &DeviceSyncBase::OnDisconnection, base::Unretained(this)));
}

DeviceSyncBase::~DeviceSyncBase() = default;

void DeviceSyncBase::AddObserver(mojom::DeviceSyncObserverPtr observer,
                                 AddObserverCallback callback) {
  observers_.AddPtr(std::move(observer));
  std::move(callback).Run();
}

void DeviceSyncBase::BindRequest(mojom::DeviceSyncRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void DeviceSyncBase::CloseAllBindings() {
  bindings_.CloseAllBindings();
}

void DeviceSyncBase::NotifyOnEnrollmentFinished() {
  observers_.ForAllPtrs(
      [](auto* observer) { observer->OnEnrollmentFinished(); });
}

void DeviceSyncBase::NotifyOnNewDevicesSynced() {
  observers_.ForAllPtrs([](auto* observer) { observer->OnNewDevicesSynced(); });
}

void DeviceSyncBase::OnDisconnection() {
  // If all clients have disconnected, shut down.
  if (bindings_.empty())
    Shutdown();
}

}  // namespace device_sync

}  // namespace chromeos
