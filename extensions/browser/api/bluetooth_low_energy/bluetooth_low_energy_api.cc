// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_api.h"

#include <stdint.h>
#include <algorithm>
#include <iterator>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "extensions/browser/api/bluetooth_low_energy/utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/kiosk/kiosk_delegate.h"
#include "extensions/common/api/bluetooth/bluetooth_manifest_data.h"
#include "extensions/common/api/bluetooth_low_energy.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/switches.h"

using content::BrowserContext;
using content::BrowserThread;

namespace apibtle = extensions::api::bluetooth_low_energy;

namespace extensions {

namespace {

const char kErrorAdapterNotInitialized[] =
    "Could not initialize Bluetooth adapter";
const char kErrorAlreadyConnected[] = "Already connected";
const char kErrorAlreadyNotifying[] = "Already notifying";
const char kErrorAuthenticationFailed[] = "Authentication failed";
const char kErrorCanceled[] = "Request canceled";
const char kErrorGattNotSupported[] = "Operation not supported by this service";
const char kErrorHigherSecurity[] = "Higher security needed";
const char kErrorInProgress[] = "In progress";
const char kErrorInsufficientAuthorization[] = "Insufficient authorization";
const char kErrorInvalidAdvertisementLength[] = "Invalid advertisement length";
const char kErrorInvalidLength[] = "Invalid attribute value length";
const char kErrorNotConnected[] = "Not connected";
const char kErrorNotFound[] = "Instance not found";
const char kErrorNotNotifying[] = "Not notifying";
const char kErrorOperationFailed[] = "Operation failed";
const char kErrorPermissionDenied[] = "Permission denied";
const char kErrorPlatformNotSupported[] =
    "This operation is not supported on the current platform";
const char kErrorTimeout[] = "Operation timed out";
const char kErrorUnsupportedDevice[] =
    "This device is not supported on the current platform";
const char kErrorInvalidServiceId[] = "The service ID doesn't exist.";
const char kErrorInvalidCharacteristicId[] =
    "The characteristic ID doesn't exist.";
const char kErrorNotifyPropertyNotSet[] =
    "The characteristic does not have the notify property set.";
const char kErrorIndicatePropertyNotSet[] =
    "The characteristic does not have the indicate property set.";
const char kErrorServiceNotRegistered[] =
    "The characteristic is not owned by a service that is registered.";
const char kErrorUnknownNotificationError[] =
    "An unknown notification error occurred.";

const char kStatusAdvertisementAlreadyExists[] =
    "An advertisement is already advertising";
const char kStatusAdvertisementDoesNotExist[] =
    "This advertisement does not exist";
#if defined(OS_LINUX)
const char kStatusInvalidAdvertisingInterval[] =
    "Invalid advertising interval specified.";
#endif

// Returns the correct error string based on error status |status|. This is used
// to set the value of |chrome.runtime.lastError.message| and should not be
// passed |BluetoothLowEnergyEventRouter::kStatusSuccess|.
std::string StatusToString(BluetoothLowEnergyEventRouter::Status status) {
  switch (status) {
    case BluetoothLowEnergyEventRouter::kStatusErrorAlreadyConnected:
      return kErrorAlreadyConnected;
    case BluetoothLowEnergyEventRouter::kStatusErrorAlreadyNotifying:
      return kErrorAlreadyNotifying;
    case BluetoothLowEnergyEventRouter::kStatusErrorAuthenticationFailed:
      return kErrorAuthenticationFailed;
    case BluetoothLowEnergyEventRouter::kStatusErrorCanceled:
      return kErrorCanceled;
    case BluetoothLowEnergyEventRouter::kStatusErrorGattNotSupported:
      return kErrorGattNotSupported;
    case BluetoothLowEnergyEventRouter::kStatusErrorHigherSecurity:
      return kErrorHigherSecurity;
    case BluetoothLowEnergyEventRouter::kStatusErrorInProgress:
      return kErrorInProgress;
    case BluetoothLowEnergyEventRouter::kStatusErrorInsufficientAuthorization:
      return kErrorInsufficientAuthorization;
    case BluetoothLowEnergyEventRouter::kStatusErrorInvalidLength:
      return kErrorInvalidLength;
    case BluetoothLowEnergyEventRouter::kStatusErrorNotConnected:
      return kErrorNotConnected;
    case BluetoothLowEnergyEventRouter::kStatusErrorNotFound:
      return kErrorNotFound;
    case BluetoothLowEnergyEventRouter::kStatusErrorNotNotifying:
      return kErrorNotNotifying;
    case BluetoothLowEnergyEventRouter::kStatusErrorPermissionDenied:
      return kErrorPermissionDenied;
    case BluetoothLowEnergyEventRouter::kStatusErrorTimeout:
      return kErrorTimeout;
    case BluetoothLowEnergyEventRouter::kStatusErrorUnsupportedDevice:
      return kErrorUnsupportedDevice;
    case BluetoothLowEnergyEventRouter::kStatusErrorInvalidServiceId:
      return kErrorInvalidServiceId;
    case BluetoothLowEnergyEventRouter::kStatusSuccess:
      NOTREACHED();
      break;
    default:
      return kErrorOperationFailed;
  }
  return "";
}

extensions::BluetoothLowEnergyEventRouter* GetEventRouter(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return extensions::BluetoothLowEnergyAPI::Get(context)->event_router();
}

template <typename T>
void DoWorkCallback(base::OnceCallback<T()> callback) {
  DCHECK(!callback.is_null());
  std::move(callback).Run();
}

std::unique_ptr<device::BluetoothAdvertisement::ManufacturerData>
CreateManufacturerData(
    std::vector<apibtle::ManufacturerData>* manufacturer_data) {
  std::unique_ptr<device::BluetoothAdvertisement::ManufacturerData>
      created_data(new device::BluetoothAdvertisement::ManufacturerData());
  for (const auto& it : *manufacturer_data) {
    std::vector<uint8_t> data(it.data.size());
    std::copy(it.data.begin(), it.data.end(), data.begin());
    (*created_data)[it.id] = data;
  }
  return created_data;
}

std::unique_ptr<device::BluetoothAdvertisement::ServiceData> CreateServiceData(
    std::vector<apibtle::ServiceData>* service_data) {
  std::unique_ptr<device::BluetoothAdvertisement::ServiceData> created_data(
      new device::BluetoothAdvertisement::ServiceData());
  for (const auto& it : *service_data) {
    std::vector<uint8_t> data(it.data.size());
    std::copy(it.data.begin(), it.data.end(), data.begin());
    (*created_data)[it.uuid] = data;
  }
  return created_data;
}

bool HasProperty(
    const std::vector<apibtle::CharacteristicProperty>& api_properties,
    apibtle::CharacteristicProperty property) {
  return find(api_properties.begin(), api_properties.end(), property) !=
         api_properties.end();
}

bool HasPermission(
    const std::vector<apibtle::DescriptorPermission>& api_permissions,
    apibtle::DescriptorPermission permission) {
  return find(api_permissions.begin(), api_permissions.end(), permission) !=
         api_permissions.end();
}

device::BluetoothGattCharacteristic::Properties GetBluetoothProperties(
    const std::vector<apibtle::CharacteristicProperty>& api_properties) {
  device::BluetoothGattCharacteristic::Properties properties =
      device::BluetoothGattCharacteristic::PROPERTY_NONE;

  static_assert(
      apibtle::CHARACTERISTIC_PROPERTY_LAST == 14,
      "Update required if the number of characteristic properties changes.");

  if (HasProperty(api_properties, apibtle::CHARACTERISTIC_PROPERTY_BROADCAST)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_BROADCAST;
  }

  if (HasProperty(api_properties, apibtle::CHARACTERISTIC_PROPERTY_READ)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_READ;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_WRITEWITHOUTRESPONSE)) {
    properties |=
        device::BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE;
  }

  if (HasProperty(api_properties, apibtle::CHARACTERISTIC_PROPERTY_WRITE)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_WRITE;
  }

  if (HasProperty(api_properties, apibtle::CHARACTERISTIC_PROPERTY_NOTIFY)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_NOTIFY;
  }

  if (HasProperty(api_properties, apibtle::CHARACTERISTIC_PROPERTY_INDICATE)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_INDICATE;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_AUTHENTICATEDSIGNEDWRITES)) {
    properties |= device::BluetoothGattCharacteristic::
        PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_EXTENDEDPROPERTIES)) {
    properties |=
        device::BluetoothGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_RELIABLEWRITE)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_RELIABLE_WRITE;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_WRITABLEAUXILIARIES)) {
    properties |=
        device::BluetoothGattCharacteristic::PROPERTY_WRITABLE_AUXILIARIES;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_ENCRYPTREAD)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_READ_ENCRYPTED;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_ENCRYPTWRITE)) {
    properties |= device::BluetoothGattCharacteristic::PROPERTY_WRITE_ENCRYPTED;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_ENCRYPTAUTHENTICATEDREAD)) {
    properties |= device::BluetoothGattCharacteristic::
        PROPERTY_READ_ENCRYPTED_AUTHENTICATED;
  }

  if (HasProperty(api_properties,
                  apibtle::CHARACTERISTIC_PROPERTY_ENCRYPTAUTHENTICATEDWRITE)) {
    properties |= device::BluetoothGattCharacteristic::
        PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED;
  }

  return properties;
}

device::BluetoothGattCharacteristic::Permissions GetBluetoothPermissions(
    const std::vector<apibtle::DescriptorPermission>& api_permissions) {
  device::BluetoothGattCharacteristic::Permissions permissions =
      device::BluetoothGattCharacteristic::PERMISSION_NONE;

  static_assert(
      apibtle::DESCRIPTOR_PERMISSION_LAST == 6,
      "Update required if the number of descriptor permissions changes.");

  if (HasPermission(api_permissions, apibtle::DESCRIPTOR_PERMISSION_READ)) {
    permissions |= device::BluetoothGattCharacteristic::PERMISSION_READ;
  }

  if (HasPermission(api_permissions, apibtle::DESCRIPTOR_PERMISSION_WRITE)) {
    permissions |= device::BluetoothGattCharacteristic::PERMISSION_WRITE;
  }

  if (HasPermission(api_permissions,
                    apibtle::DESCRIPTOR_PERMISSION_ENCRYPTEDREAD)) {
    permissions |=
        device::BluetoothGattCharacteristic::PERMISSION_READ_ENCRYPTED;
  }

  if (HasPermission(api_permissions,
                    apibtle::DESCRIPTOR_PERMISSION_ENCRYPTEDWRITE)) {
    permissions |=
        device::BluetoothGattCharacteristic::PERMISSION_WRITE_ENCRYPTED;
  }

  if (HasPermission(
          api_permissions,
          apibtle::DESCRIPTOR_PERMISSION_ENCRYPTEDAUTHENTICATEDREAD)) {
    permissions |= device::BluetoothGattCharacteristic::
        PERMISSION_READ_ENCRYPTED_AUTHENTICATED;
  }

  if (HasPermission(
          api_permissions,
          apibtle::DESCRIPTOR_PERMISSION_ENCRYPTEDAUTHENTICATEDWRITE)) {
    permissions |= device::BluetoothGattCharacteristic::
        PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED;
  }

  return permissions;
}

bool IsAutoLaunchedKioskApp(const ExtensionId& id) {
  KioskDelegate* delegate = ExtensionsBrowserClient::Get()->GetKioskDelegate();
  DCHECK(delegate);
  return delegate->IsAutoLaunchedKioskApp(id);
}

bool IsPeripheralFlagEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBLEAdvertising);
}

}  // namespace

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>>::DestructorAtExit
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>*
BluetoothLowEnergyAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
BluetoothLowEnergyAPI* BluetoothLowEnergyAPI::Get(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetFactoryInstance()->Get(context);
}

BluetoothLowEnergyAPI::BluetoothLowEnergyAPI(BrowserContext* context)
    : event_router_(new BluetoothLowEnergyEventRouter(context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BluetoothLowEnergyAPI::~BluetoothLowEnergyAPI() {}

void BluetoothLowEnergyAPI::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

namespace api {

BluetoothLowEnergyExtensionFunction::BluetoothLowEnergyExtensionFunction()
    : event_router_(nullptr) {}

BluetoothLowEnergyExtensionFunction::~BluetoothLowEnergyExtensionFunction() {}

ExtensionFunction::ResponseAction BluetoothLowEnergyExtensionFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  EXTENSION_FUNCTION_VALIDATE(ParseParams());

  if (!BluetoothManifestData::CheckLowEnergyPermitted(extension()))
    return RespondNow(Error(kErrorPermissionDenied));

  event_router_ = GetEventRouter(browser_context());
  if (!event_router_->IsBluetoothSupported())
    return RespondNow(Error(kErrorPlatformNotSupported));

  // It is safe to pass |this| here as ExtensionFunction is refcounted.
  if (!event_router_->InitializeAdapterAndInvokeCallback(base::BindOnce(
          &DoWorkCallback<void>,
          base::BindOnce(&BluetoothLowEnergyExtensionFunction::PreDoWork,
                         this)))) {
    // DoWork will respond when the adapter gets initialized.
    return RespondNow(Error(kErrorAdapterNotInitialized));
  }

  return RespondLater();
}

void BluetoothLowEnergyExtensionFunction::PreDoWork() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router_->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }
  DoWork();
}

BLEPeripheralExtensionFunction::BLEPeripheralExtensionFunction() {}

BLEPeripheralExtensionFunction::~BLEPeripheralExtensionFunction() {}

ExtensionFunction::ResponseAction BLEPeripheralExtensionFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check permissions in manifest.
  if (!BluetoothManifestData::CheckPeripheralPermitted(extension()))
    return RespondNow(Error(kErrorPermissionDenied));

  if (!(IsAutoLaunchedKioskApp(extension()->id()) ||
        IsPeripheralFlagEnabled())) {
    return RespondNow(Error(kErrorPermissionDenied));
  }

  return BluetoothLowEnergyExtensionFunction::Run();
}

BluetoothLowEnergyConnectFunction::BluetoothLowEnergyConnectFunction() {}

BluetoothLowEnergyConnectFunction::~BluetoothLowEnergyConnectFunction() {}

bool BluetoothLowEnergyConnectFunction::ParseParams() {
  params_ = apibtle::Connect::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyConnectFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  bool persistent = false;  // Not persistent by default.
  apibtle::ConnectProperties* properties = params_->properties.get();
  if (properties)
    persistent = properties->persistent;

  event_router->Connect(
      persistent, extension(), params_->device_address,
      base::Bind(&BluetoothLowEnergyConnectFunction::SuccessCallback, this),
      base::Bind(&BluetoothLowEnergyConnectFunction::ErrorCallback, this));
}

void BluetoothLowEnergyConnectFunction::SuccessCallback() {
  Respond(NoArguments());
}

void BluetoothLowEnergyConnectFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

BluetoothLowEnergyDisconnectFunction::BluetoothLowEnergyDisconnectFunction() {}

BluetoothLowEnergyDisconnectFunction::~BluetoothLowEnergyDisconnectFunction() {}

bool BluetoothLowEnergyDisconnectFunction::ParseParams() {
  params_ = apibtle::Disconnect::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyDisconnectFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  event_router->Disconnect(
      extension(), params_->device_address,
      base::Bind(&BluetoothLowEnergyDisconnectFunction::SuccessCallback, this),
      base::Bind(&BluetoothLowEnergyDisconnectFunction::ErrorCallback, this));
}

void BluetoothLowEnergyDisconnectFunction::SuccessCallback() {
  Respond(NoArguments());
}

void BluetoothLowEnergyDisconnectFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

BluetoothLowEnergyGetServiceFunction::BluetoothLowEnergyGetServiceFunction() {}

BluetoothLowEnergyGetServiceFunction::~BluetoothLowEnergyGetServiceFunction() {}

bool BluetoothLowEnergyGetServiceFunction::ParseParams() {
  params_ = apibtle::GetService::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyGetServiceFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  apibtle::Service service;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetService(params_->service_id, &service);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    Respond(Error(StatusToString(status)));
    return;
  }

  Respond(ArgumentList(apibtle::GetService::Results::Create(service)));
}

BluetoothLowEnergyGetServicesFunction::BluetoothLowEnergyGetServicesFunction() {
}

BluetoothLowEnergyGetServicesFunction::
    ~BluetoothLowEnergyGetServicesFunction() {}

bool BluetoothLowEnergyGetServicesFunction::ParseParams() {
  params_ = apibtle::GetServices::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyGetServicesFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  BluetoothLowEnergyEventRouter::ServiceList service_list;
  if (!event_router->GetServices(params_->device_address, &service_list)) {
    Respond(Error(kErrorNotFound));
    return;
  }

  Respond(ArgumentList(apibtle::GetServices::Results::Create(service_list)));
}

BluetoothLowEnergyGetCharacteristicFunction::
    BluetoothLowEnergyGetCharacteristicFunction() {}

BluetoothLowEnergyGetCharacteristicFunction::
    ~BluetoothLowEnergyGetCharacteristicFunction() {}

bool BluetoothLowEnergyGetCharacteristicFunction::ParseParams() {
  params_ = apibtle::GetCharacteristic::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyGetCharacteristicFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  apibtle::Characteristic characteristic;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetCharacteristic(extension(), params_->characteristic_id,
                                      &characteristic);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    Respond(Error(StatusToString(status)));
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetCharacteristic::Result::Create as it doesn't convert lists of
  // enums correctly.
  Respond(OneArgument(apibtle::CharacteristicToValue(&characteristic)));
}

BluetoothLowEnergyGetCharacteristicsFunction::
    BluetoothLowEnergyGetCharacteristicsFunction() {}

BluetoothLowEnergyGetCharacteristicsFunction::
    ~BluetoothLowEnergyGetCharacteristicsFunction() {}

bool BluetoothLowEnergyGetCharacteristicsFunction::ParseParams() {
  params_ = apibtle::GetCharacteristics::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyGetCharacteristicsFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  BluetoothLowEnergyEventRouter::CharacteristicList characteristic_list;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetCharacteristics(extension(), params_->service_id,
                                       &characteristic_list);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    Respond(Error(StatusToString(status)));
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetCharacteristics::Result::Create as it doesn't convert lists of
  // enums correctly.
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (apibtle::Characteristic& characteristic : characteristic_list)
    result->Append(apibtle::CharacteristicToValue(&characteristic));

  Respond(OneArgument(std::move(result)));
}

BluetoothLowEnergyGetIncludedServicesFunction::
    BluetoothLowEnergyGetIncludedServicesFunction() {}

BluetoothLowEnergyGetIncludedServicesFunction::
    ~BluetoothLowEnergyGetIncludedServicesFunction() {}

bool BluetoothLowEnergyGetIncludedServicesFunction::ParseParams() {
  params_ = apibtle::GetIncludedServices::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyGetIncludedServicesFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  BluetoothLowEnergyEventRouter::ServiceList service_list;
  BluetoothLowEnergyEventRouter::Status status =
      event_router->GetIncludedServices(params_->service_id, &service_list);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    Respond(Error(StatusToString(status)));
    return;
  }

  Respond(ArgumentList(
      apibtle::GetIncludedServices::Results::Create(service_list)));
}

BluetoothLowEnergyGetDescriptorFunction::
    BluetoothLowEnergyGetDescriptorFunction() {}

BluetoothLowEnergyGetDescriptorFunction::
    ~BluetoothLowEnergyGetDescriptorFunction() {}

bool BluetoothLowEnergyGetDescriptorFunction::ParseParams() {
  params_ = apibtle::GetDescriptor::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyGetDescriptorFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  apibtle::Descriptor descriptor;
  BluetoothLowEnergyEventRouter::Status status = event_router->GetDescriptor(
      extension(), params_->descriptor_id, &descriptor);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    Respond(Error(StatusToString(status)));
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetDescriptor::Result::Create as it doesn't convert lists of enums
  // correctly.
  Respond(OneArgument(apibtle::DescriptorToValue(&descriptor)));
}

BluetoothLowEnergyGetDescriptorsFunction::
    BluetoothLowEnergyGetDescriptorsFunction() {}

BluetoothLowEnergyGetDescriptorsFunction::
    ~BluetoothLowEnergyGetDescriptorsFunction() {}

bool BluetoothLowEnergyGetDescriptorsFunction::ParseParams() {
  params_ = apibtle::GetDescriptors::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyGetDescriptorsFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  BluetoothLowEnergyEventRouter::DescriptorList descriptor_list;
  BluetoothLowEnergyEventRouter::Status status = event_router->GetDescriptors(
      extension(), params_->characteristic_id, &descriptor_list);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    Respond(Error(StatusToString(status)));
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetDescriptors::Result::Create as it doesn't convert lists of
  // enums correctly.
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (apibtle::Descriptor& descriptor : descriptor_list)
    result->Append(apibtle::DescriptorToValue(&descriptor));

  Respond(OneArgument(std::move(result)));
}

BluetoothLowEnergyReadCharacteristicValueFunction::
    BluetoothLowEnergyReadCharacteristicValueFunction() {}

BluetoothLowEnergyReadCharacteristicValueFunction::
    ~BluetoothLowEnergyReadCharacteristicValueFunction() {}

bool BluetoothLowEnergyReadCharacteristicValueFunction::ParseParams() {
  params_ = apibtle::ReadCharacteristicValue::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyReadCharacteristicValueFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  instance_id_ = params_->characteristic_id;
  event_router->ReadCharacteristicValue(
      extension(), instance_id_,
      base::Bind(
          &BluetoothLowEnergyReadCharacteristicValueFunction::SuccessCallback,
          this),
      base::Bind(
          &BluetoothLowEnergyReadCharacteristicValueFunction::ErrorCallback,
          this));
}

void BluetoothLowEnergyReadCharacteristicValueFunction::SuccessCallback() {
  // Obtain info on the characteristic and see whether or not the characteristic
  // is still around.
  apibtle::Characteristic characteristic;
  BluetoothLowEnergyEventRouter::Status status =
      GetEventRouter(browser_context())
          ->GetCharacteristic(extension(), instance_id_, &characteristic);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    Respond(Error(StatusToString(status)));
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetCharacteristic::Result::Create as it doesn't convert lists of
  // enums correctly.
  Respond(OneArgument(apibtle::CharacteristicToValue(&characteristic)));
}

void BluetoothLowEnergyReadCharacteristicValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

BluetoothLowEnergyWriteCharacteristicValueFunction::
    BluetoothLowEnergyWriteCharacteristicValueFunction() {}

BluetoothLowEnergyWriteCharacteristicValueFunction::
    ~BluetoothLowEnergyWriteCharacteristicValueFunction() {}

bool BluetoothLowEnergyWriteCharacteristicValueFunction::ParseParams() {
  params_ = apibtle::WriteCharacteristicValue::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyWriteCharacteristicValueFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  std::vector<uint8_t> value(params_->value.begin(), params_->value.end());
  event_router->WriteCharacteristicValue(
      extension(), params_->characteristic_id, value,
      base::Bind(
          &BluetoothLowEnergyWriteCharacteristicValueFunction::SuccessCallback,
          this),
      base::Bind(
          &BluetoothLowEnergyWriteCharacteristicValueFunction::ErrorCallback,
          this));
}

void BluetoothLowEnergyWriteCharacteristicValueFunction::SuccessCallback() {
  Respond(ArgumentList(apibtle::WriteCharacteristicValue::Results::Create()));
}

void BluetoothLowEnergyWriteCharacteristicValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

BluetoothLowEnergyStartCharacteristicNotificationsFunction::
    BluetoothLowEnergyStartCharacteristicNotificationsFunction() {}

BluetoothLowEnergyStartCharacteristicNotificationsFunction::
    ~BluetoothLowEnergyStartCharacteristicNotificationsFunction() {}

bool BluetoothLowEnergyStartCharacteristicNotificationsFunction::ParseParams() {
  params_ = apibtle::StartCharacteristicNotifications::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyStartCharacteristicNotificationsFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  bool persistent = false;  // Not persistent by default.
  apibtle::NotificationProperties* properties = params_->properties.get();
  if (properties)
    persistent = properties->persistent;

  event_router->StartCharacteristicNotifications(
      persistent, extension(), params_->characteristic_id,
      base::Bind(&BluetoothLowEnergyStartCharacteristicNotificationsFunction::
                     SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyStartCharacteristicNotificationsFunction::
                     ErrorCallback,
                 this));
}

void BluetoothLowEnergyStartCharacteristicNotificationsFunction::
    SuccessCallback() {
  Respond(NoArguments());
}

void BluetoothLowEnergyStartCharacteristicNotificationsFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

BluetoothLowEnergyStopCharacteristicNotificationsFunction::
    BluetoothLowEnergyStopCharacteristicNotificationsFunction() {}

BluetoothLowEnergyStopCharacteristicNotificationsFunction::
    ~BluetoothLowEnergyStopCharacteristicNotificationsFunction() {}

bool BluetoothLowEnergyStopCharacteristicNotificationsFunction::ParseParams() {
  params_ = apibtle::StopCharacteristicNotifications::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyStopCharacteristicNotificationsFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  event_router->StopCharacteristicNotifications(
      extension(), params_->characteristic_id,
      base::Bind(&BluetoothLowEnergyStopCharacteristicNotificationsFunction::
                     SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyStopCharacteristicNotificationsFunction::
                     ErrorCallback,
                 this));
}

void BluetoothLowEnergyStopCharacteristicNotificationsFunction::
    SuccessCallback() {
  Respond(NoArguments());
}

void BluetoothLowEnergyStopCharacteristicNotificationsFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

BluetoothLowEnergyReadDescriptorValueFunction::
    BluetoothLowEnergyReadDescriptorValueFunction() {}

BluetoothLowEnergyReadDescriptorValueFunction::
    ~BluetoothLowEnergyReadDescriptorValueFunction() {}

bool BluetoothLowEnergyReadDescriptorValueFunction::ParseParams() {
  params_ = apibtle::ReadDescriptorValue::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyReadDescriptorValueFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  instance_id_ = params_->descriptor_id;
  event_router->ReadDescriptorValue(
      extension(), instance_id_,
      base::Bind(
          &BluetoothLowEnergyReadDescriptorValueFunction::SuccessCallback,
          this),
      base::Bind(&BluetoothLowEnergyReadDescriptorValueFunction::ErrorCallback,
                 this));
}

void BluetoothLowEnergyReadDescriptorValueFunction::SuccessCallback() {
  // Obtain info on the descriptor and see whether or not the descriptor is
  // still around.
  apibtle::Descriptor descriptor;
  BluetoothLowEnergyEventRouter::Status status =
      GetEventRouter(browser_context())
          ->GetDescriptor(extension(), instance_id_, &descriptor);
  if (status != BluetoothLowEnergyEventRouter::kStatusSuccess) {
    Respond(Error(StatusToString(status)));
    return;
  }

  // Manually construct the result instead of using
  // apibtle::GetDescriptor::Results::Create as it doesn't convert lists of
  // enums correctly.
  Respond(OneArgument(apibtle::DescriptorToValue(&descriptor)));
}

void BluetoothLowEnergyReadDescriptorValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

BluetoothLowEnergyWriteDescriptorValueFunction::
    BluetoothLowEnergyWriteDescriptorValueFunction() {}

BluetoothLowEnergyWriteDescriptorValueFunction::
    ~BluetoothLowEnergyWriteDescriptorValueFunction() {}

bool BluetoothLowEnergyWriteDescriptorValueFunction::ParseParams() {
  params_ = apibtle::WriteDescriptorValue::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyWriteDescriptorValueFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  std::vector<uint8_t> value(params_->value.begin(), params_->value.end());
  event_router->WriteDescriptorValue(
      extension(), params_->descriptor_id, value,
      base::Bind(
          &BluetoothLowEnergyWriteDescriptorValueFunction::SuccessCallback,
          this),
      base::Bind(&BluetoothLowEnergyWriteDescriptorValueFunction::ErrorCallback,
                 this));
}

void BluetoothLowEnergyWriteDescriptorValueFunction::SuccessCallback() {
  Respond(ArgumentList(apibtle::WriteDescriptorValue::Results::Create()));
}

void BluetoothLowEnergyWriteDescriptorValueFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

BluetoothLowEnergyAdvertisementFunction::
    BluetoothLowEnergyAdvertisementFunction()
    : advertisements_manager_(nullptr) {}

BluetoothLowEnergyAdvertisementFunction::
    ~BluetoothLowEnergyAdvertisementFunction() {}

int BluetoothLowEnergyAdvertisementFunction::AddAdvertisement(
    BluetoothApiAdvertisement* advertisement) {
  DCHECK(advertisements_manager_);
  return advertisements_manager_->Add(advertisement);
}

BluetoothApiAdvertisement*
BluetoothLowEnergyAdvertisementFunction::GetAdvertisement(
    int advertisement_id) {
  DCHECK(advertisements_manager_);
  return advertisements_manager_->Get(extension_id(), advertisement_id);
}

void BluetoothLowEnergyAdvertisementFunction::RemoveAdvertisement(
    int advertisement_id) {
  DCHECK(advertisements_manager_);
  advertisements_manager_->Remove(extension_id(), advertisement_id);
}

const std::unordered_set<int>*
BluetoothLowEnergyAdvertisementFunction::GetAdvertisementIds() {
  return advertisements_manager_->GetResourceIds(extension_id());
}

ExtensionFunction::ResponseAction
BluetoothLowEnergyAdvertisementFunction::Run() {
  Initialize();

  return BLEPeripheralExtensionFunction::Run();
}

void BluetoothLowEnergyAdvertisementFunction::Initialize() {
  advertisements_manager_ =
      ApiResourceManager<BluetoothApiAdvertisement>::Get(browser_context());
}

// RegisterAdvertisement:

BluetoothLowEnergyRegisterAdvertisementFunction::
    BluetoothLowEnergyRegisterAdvertisementFunction() {}

BluetoothLowEnergyRegisterAdvertisementFunction::
    ~BluetoothLowEnergyRegisterAdvertisementFunction() {}

bool BluetoothLowEnergyRegisterAdvertisementFunction::ParseParams() {
  params_ = apibtle::RegisterAdvertisement::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyRegisterAdvertisementFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // The adapter must be initialized at this point, but return an error instead
  // of asserting.
  if (!event_router->HasAdapter()) {
    Respond(Error(kErrorAdapterNotInitialized));
    return;
  }

  std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data(
      new device::BluetoothAdvertisement::Data(
          params_->advertisement.type ==
                  apibtle::AdvertisementType::ADVERTISEMENT_TYPE_BROADCAST
              ? device::BluetoothAdvertisement::AdvertisementType::
                    ADVERTISEMENT_TYPE_BROADCAST
              : device::BluetoothAdvertisement::AdvertisementType::
                    ADVERTISEMENT_TYPE_PERIPHERAL));

  advertisement_data->set_service_uuids(
      std::move(params_->advertisement.service_uuids));
  advertisement_data->set_solicit_uuids(
      std::move(params_->advertisement.solicit_uuids));
  if (params_->advertisement.manufacturer_data) {
    advertisement_data->set_manufacturer_data(
        CreateManufacturerData(params_->advertisement.manufacturer_data.get()));
  }
  if (params_->advertisement.service_data) {
    advertisement_data->set_service_data(
        CreateServiceData(params_->advertisement.service_data.get()));
  }

  event_router->adapter()->RegisterAdvertisement(
      std::move(advertisement_data),
      base::Bind(
          &BluetoothLowEnergyRegisterAdvertisementFunction::SuccessCallback,
          this),
      base::Bind(
          &BluetoothLowEnergyRegisterAdvertisementFunction::ErrorCallback,
          this));
}

void BluetoothLowEnergyRegisterAdvertisementFunction::SuccessCallback(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  Respond(ArgumentList(
      apibtle::RegisterAdvertisement::Results::Create(AddAdvertisement(
          new BluetoothApiAdvertisement(extension_id(), advertisement)))));
}

void BluetoothLowEnergyRegisterAdvertisementFunction::ErrorCallback(
    device::BluetoothAdvertisement::ErrorCode status) {
  switch (status) {
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_ALREADY_EXISTS:
      Respond(Error(kStatusAdvertisementAlreadyExists));
      break;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_INVALID_LENGTH:
      Respond(Error(kErrorInvalidAdvertisementLength));
      break;
    default:
      Respond(Error(kErrorOperationFailed));
  }
}

// UnregisterAdvertisement:

BluetoothLowEnergyUnregisterAdvertisementFunction::
    BluetoothLowEnergyUnregisterAdvertisementFunction() {}

BluetoothLowEnergyUnregisterAdvertisementFunction::
    ~BluetoothLowEnergyUnregisterAdvertisementFunction() {}

bool BluetoothLowEnergyUnregisterAdvertisementFunction::ParseParams() {
  params_ = apibtle::UnregisterAdvertisement::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyUnregisterAdvertisementFunction::DoWork() {
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // If we don't have an initialized adapter, unregistering is a no-op.
  if (!event_router->HasAdapter()) {
    Respond(NoArguments());
    return;
  }

  BluetoothApiAdvertisement* advertisement =
      GetAdvertisement(params_->advertisement_id);
  if (!advertisement) {
    Respond(Error(kStatusAdvertisementDoesNotExist));
    return;
  }

  advertisement->advertisement()->Unregister(
      base::Bind(
          &BluetoothLowEnergyUnregisterAdvertisementFunction::SuccessCallback,
          this, params_->advertisement_id),
      base::Bind(
          &BluetoothLowEnergyUnregisterAdvertisementFunction::ErrorCallback,
          this, params_->advertisement_id));
}

void BluetoothLowEnergyUnregisterAdvertisementFunction::SuccessCallback(
    int advertisement_id) {
  RemoveAdvertisement(advertisement_id);
  Respond(NoArguments());
}

void BluetoothLowEnergyUnregisterAdvertisementFunction::ErrorCallback(
    int advertisement_id,
    device::BluetoothAdvertisement::ErrorCode status) {
  RemoveAdvertisement(advertisement_id);
  switch (status) {
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_DOES_NOT_EXIST:
      Respond(Error(kStatusAdvertisementDoesNotExist));
      break;
    default:
      Respond(Error(kErrorOperationFailed));
  }
}

// ResetAdvertising:

BluetoothLowEnergyResetAdvertisingFunction::
    BluetoothLowEnergyResetAdvertisingFunction() {}

BluetoothLowEnergyResetAdvertisingFunction::
    ~BluetoothLowEnergyResetAdvertisingFunction() {}

bool BluetoothLowEnergyResetAdvertisingFunction::ParseParams() {
  return true;
}

void BluetoothLowEnergyResetAdvertisingFunction::DoWork() {
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());

  // If the adapter is not initialized, there is nothing to reset.
  if (!event_router->HasAdapter()) {
    Respond(NoArguments());
    return;
  }

  const std::unordered_set<int>* advertisement_ids = GetAdvertisementIds();
  if (!advertisement_ids || advertisement_ids->empty()) {
    Respond(NoArguments());
    return;
  }

  // Copy the hash set, as RemoveAdvertisement can change advertisement_ids
  // while we are iterating over it.
  std::unordered_set<int> advertisement_ids_tmp = *advertisement_ids;
  for (int advertisement_id : advertisement_ids_tmp) {
    RemoveAdvertisement(advertisement_id);
  }

  event_router->adapter()->ResetAdvertising(
      base::Bind(&BluetoothLowEnergyResetAdvertisingFunction::SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyResetAdvertisingFunction::ErrorCallback,
                 this));
#endif
}

void BluetoothLowEnergyResetAdvertisingFunction::SuccessCallback() {
  Respond(NoArguments());
}

void BluetoothLowEnergyResetAdvertisingFunction::ErrorCallback(
    device::BluetoothAdvertisement::ErrorCode status) {
  Respond(Error(kErrorOperationFailed));
}

// SetAdvertisingInterval:

BluetoothLowEnergySetAdvertisingIntervalFunction::
    BluetoothLowEnergySetAdvertisingIntervalFunction() {}

BluetoothLowEnergySetAdvertisingIntervalFunction::
    ~BluetoothLowEnergySetAdvertisingIntervalFunction() {}

bool BluetoothLowEnergySetAdvertisingIntervalFunction::ParseParams() {
  params_ = apibtle::SetAdvertisingInterval::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergySetAdvertisingIntervalFunction::DoWork() {
#if defined(OS_LINUX)
  BluetoothLowEnergyEventRouter* event_router =
      GetEventRouter(browser_context());
  event_router->adapter()->SetAdvertisingInterval(
      base::TimeDelta::FromMilliseconds(params_->min_interval),
      base::TimeDelta::FromMilliseconds(params_->max_interval),
      base::Bind(
          &BluetoothLowEnergySetAdvertisingIntervalFunction::SuccessCallback,
          this),
      base::Bind(
          &BluetoothLowEnergySetAdvertisingIntervalFunction::ErrorCallback,
          this));
#endif
}

void BluetoothLowEnergySetAdvertisingIntervalFunction::SuccessCallback() {
  Respond(NoArguments());
}

void BluetoothLowEnergySetAdvertisingIntervalFunction::ErrorCallback(
    device::BluetoothAdvertisement::ErrorCode status) {
#if defined(OS_LINUX)
  switch (status) {
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_INVALID_ADVERTISEMENT_INTERVAL:
      Respond(Error(kStatusInvalidAdvertisingInterval));
      break;
    default:
      Respond(Error(kErrorOperationFailed));
  }
#endif
}

// createService:

BluetoothLowEnergyCreateServiceFunction::
    BluetoothLowEnergyCreateServiceFunction() {}

BluetoothLowEnergyCreateServiceFunction::
    ~BluetoothLowEnergyCreateServiceFunction() {}

bool BluetoothLowEnergyCreateServiceFunction::ParseParams() {
// Causes link error on Windows. API will never be on Windows, so #ifdefing.
#if !defined(OS_WIN)
  params_ = apibtle::CreateService::Params::Create(*args_);
  return params_.get() != nullptr;
#else
  return true;
#endif
}

void BluetoothLowEnergyCreateServiceFunction::DoWork() {
// Causes link error on Windows. API will never be on Windows, so #ifdefing.
// TODO: Ideally this should be handled by our feature system, so that this
// code doesn't even compile on OSes it isn't being used on, but currently this
// is not possible.
#if !defined(OS_WIN)
  base::WeakPtr<device::BluetoothLocalGattService> service =
      device::BluetoothLocalGattService::Create(
          event_router_->adapter(),
          device::BluetoothUUID(params_->service.uuid),
          params_->service.is_primary, nullptr, event_router_);

  event_router_->AddServiceToApp(extension_id(), service->GetIdentifier());
  Respond(ArgumentList(
      apibtle::CreateService::Results::Create(service->GetIdentifier())));
#else
  Respond(Error(kErrorPlatformNotSupported));
#endif
}

// createCharacteristic:

BluetoothLowEnergyCreateCharacteristicFunction::
    BluetoothLowEnergyCreateCharacteristicFunction() {}

BluetoothLowEnergyCreateCharacteristicFunction::
    ~BluetoothLowEnergyCreateCharacteristicFunction() {}

bool BluetoothLowEnergyCreateCharacteristicFunction::ParseParams() {
  params_ = apibtle::CreateCharacteristic::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyCreateCharacteristicFunction::DoWork() {
  device::BluetoothLocalGattService* service =
      event_router_->adapter()->GetGattService(params_->service_id);
  if (!service) {
    Respond(Error(kErrorInvalidServiceId));
    return;
  }

  base::WeakPtr<device::BluetoothLocalGattCharacteristic> characteristic =
      device::BluetoothLocalGattCharacteristic::Create(
          device::BluetoothUUID(params_->characteristic.uuid),
          GetBluetoothProperties(params_->characteristic.properties),
          device::BluetoothGattCharacteristic::Permissions(), service);

  // Keep a track of this characteristic so we can look it up later if a
  // descriptor lists it as its parent.
  event_router_->AddLocalCharacteristic(characteristic->GetIdentifier(),
                                        service->GetIdentifier());

  Respond(ArgumentList(apibtle::CreateCharacteristic::Results::Create(
      characteristic->GetIdentifier())));
}

// createDescriptor:

BluetoothLowEnergyCreateDescriptorFunction::
    BluetoothLowEnergyCreateDescriptorFunction() {}

BluetoothLowEnergyCreateDescriptorFunction::
    ~BluetoothLowEnergyCreateDescriptorFunction() {}

bool BluetoothLowEnergyCreateDescriptorFunction::ParseParams() {
  params_ = apibtle::CreateDescriptor::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyCreateDescriptorFunction::DoWork() {
  device::BluetoothLocalGattCharacteristic* characteristic =
      event_router_->GetLocalCharacteristic(params_->characteristic_id);
  if (!characteristic) {
    Respond(Error(kErrorInvalidCharacteristicId));
    return;
  }

  base::WeakPtr<device::BluetoothLocalGattDescriptor> descriptor =
      device::BluetoothLocalGattDescriptor::Create(
          device::BluetoothUUID(params_->descriptor.uuid),
          GetBluetoothPermissions(params_->descriptor.permissions),
          characteristic);

  Respond(ArgumentList(
      apibtle::CreateDescriptor::Results::Create(descriptor->GetIdentifier())));
}

// registerService:

BluetoothLowEnergyRegisterServiceFunction::
    BluetoothLowEnergyRegisterServiceFunction() {}

BluetoothLowEnergyRegisterServiceFunction::
    ~BluetoothLowEnergyRegisterServiceFunction() {}

bool BluetoothLowEnergyRegisterServiceFunction::ParseParams() {
  params_ = apibtle::RegisterService::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyRegisterServiceFunction::DoWork() {
  event_router_->RegisterGattService(
      extension(), params_->service_id,
      base::Bind(&BluetoothLowEnergyRegisterServiceFunction::SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyRegisterServiceFunction::ErrorCallback,
                 this));
}

void BluetoothLowEnergyRegisterServiceFunction::SuccessCallback() {
  Respond(NoArguments());
}

void BluetoothLowEnergyRegisterServiceFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

// unregisterService:

BluetoothLowEnergyUnregisterServiceFunction::
    BluetoothLowEnergyUnregisterServiceFunction() {}

BluetoothLowEnergyUnregisterServiceFunction::
    ~BluetoothLowEnergyUnregisterServiceFunction() {}

bool BluetoothLowEnergyUnregisterServiceFunction::ParseParams() {
  params_ = apibtle::UnregisterService::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyUnregisterServiceFunction::DoWork() {
  event_router_->UnregisterGattService(
      extension(), params_->service_id,
      base::Bind(&BluetoothLowEnergyUnregisterServiceFunction::SuccessCallback,
                 this),
      base::Bind(&BluetoothLowEnergyUnregisterServiceFunction::ErrorCallback,
                 this));
}

void BluetoothLowEnergyUnregisterServiceFunction::SuccessCallback() {
  Respond(NoArguments());
}

void BluetoothLowEnergyUnregisterServiceFunction::ErrorCallback(
    BluetoothLowEnergyEventRouter::Status status) {
  Respond(Error(StatusToString(status)));
}

// notifyCharacteristicValueChanged:

BluetoothLowEnergyNotifyCharacteristicValueChangedFunction::
    BluetoothLowEnergyNotifyCharacteristicValueChangedFunction() {}

BluetoothLowEnergyNotifyCharacteristicValueChangedFunction::
    ~BluetoothLowEnergyNotifyCharacteristicValueChangedFunction() {}

bool BluetoothLowEnergyNotifyCharacteristicValueChangedFunction::ParseParams() {
  params_ = apibtle::NotifyCharacteristicValueChanged::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyNotifyCharacteristicValueChangedFunction::DoWork() {
  device::BluetoothLocalGattCharacteristic* characteristic =
      event_router_->GetLocalCharacteristic(params_->characteristic_id);
  if (!characteristic) {
    Respond(Error(kErrorInvalidCharacteristicId));
    return;
  }
  std::vector<uint8_t> uint8_vector;
  uint8_vector.assign(params_->notification.value.begin(),
                      params_->notification.value.end());

  bool indicate = params_->notification.should_indicate.get()
                      ? *params_->notification.should_indicate
                      : false;
  device::BluetoothLocalGattCharacteristic::NotificationStatus status =
      characteristic->NotifyValueChanged(nullptr, uint8_vector, indicate);

  switch (status) {
    case device::BluetoothLocalGattCharacteristic::NOTIFICATION_SUCCESS:
      Respond(NoArguments());
      break;
    case device::BluetoothLocalGattCharacteristic::NOTIFY_PROPERTY_NOT_SET:
      Respond(Error(kErrorNotifyPropertyNotSet));
      break;
    case device::BluetoothLocalGattCharacteristic::INDICATE_PROPERTY_NOT_SET:
      Respond(Error(kErrorIndicatePropertyNotSet));
      break;
    case device::BluetoothLocalGattCharacteristic::SERVICE_NOT_REGISTERED:
      Respond(Error(kErrorServiceNotRegistered));
      break;
    default:
      LOG(ERROR) << "Unknown notification error!";
      Respond(Error(kErrorUnknownNotificationError));
  }
}

// removeService:

BluetoothLowEnergyRemoveServiceFunction::
    BluetoothLowEnergyRemoveServiceFunction() {}

BluetoothLowEnergyRemoveServiceFunction::
    ~BluetoothLowEnergyRemoveServiceFunction() {}

bool BluetoothLowEnergyRemoveServiceFunction::ParseParams() {
  params_ = apibtle::RemoveService::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergyRemoveServiceFunction::DoWork() {
  device::BluetoothLocalGattService* service =
      event_router_->adapter()->GetGattService(params_->service_id);
  if (!service) {
    Respond(Error(kErrorInvalidServiceId));
    return;
  }
  event_router_->RemoveServiceFromApp(extension_id(), service->GetIdentifier());
  service->Delete();
  Respond(NoArguments());
}

// sendRequestResponse:

BluetoothLowEnergySendRequestResponseFunction::
    BluetoothLowEnergySendRequestResponseFunction() {}

BluetoothLowEnergySendRequestResponseFunction::
    ~BluetoothLowEnergySendRequestResponseFunction() {}

bool BluetoothLowEnergySendRequestResponseFunction::ParseParams() {
  params_ = apibtle::SendRequestResponse::Params::Create(*args_);
  return params_.get() != nullptr;
}

void BluetoothLowEnergySendRequestResponseFunction::DoWork() {
  std::vector<uint8_t> uint8_vector;
  if (params_->response.value) {
    uint8_vector.assign(params_->response.value->begin(),
                        params_->response.value->end());
  }
  event_router_->HandleRequestResponse(
      extension(), params_->response.request_id, params_->response.is_error,
      uint8_vector);
  Respond(NoArguments());
}

}  // namespace api
}  // namespace extensions
