// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_SERVICE_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_SERVICE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace chromeos {

namespace device_sync {

class ClientAppMetadataProvider;
class DeviceSyncBase;
class GcmDeviceInfoProvider;

// Service which provides an implementation for DeviceSync. This service creates
// one implementation and shares it among all connection requests.
class DeviceSyncService : public mojom::DeviceSyncServiceInitializer,
                          public mojom::DeviceSyncService {
 public:
  DeviceSyncService(
      signin::IdentityManager* identity_manager,
      gcm::GCMDriver* gcm_driver,
      const GcmDeviceInfoProvider* gcm_device_info_provider,
      ClientAppMetadataProvider* client_app_metadata_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      mojo::PendingReceiver<mojom::DeviceSyncServiceInitializer> init_receiver);
  ~DeviceSyncService() override;

 private:
  // mojom::DeviceSyncServiceInitializer:
  void Initialize(mojo::PendingReceiver<mojom::DeviceSyncService> receiver,
                  mojo::PendingRemote<prefs::mojom::PrefStoreConnector>
                      pref_store_connector) override;

  // mojom::DeviceSyncService:
  void BindDeviceSync(
      mojo::PendingReceiver<mojom::DeviceSync> receiver) override;

  mojo::Receiver<mojom::DeviceSyncServiceInitializer> init_receiver_;
  mojo::Receiver<mojom::DeviceSyncService> receiver_{this};

  signin::IdentityManager* identity_manager_;
  gcm::GCMDriver* gcm_driver_;
  const GcmDeviceInfoProvider* gcm_device_info_provider_;
  ClientAppMetadataProvider* client_app_metadata_provider_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<DeviceSyncBase> device_sync_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncService);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_SERVICE_H_
