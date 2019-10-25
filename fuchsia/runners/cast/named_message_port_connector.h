// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_H_
#define FUCHSIA_RUNNERS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"

// Injects an API into |frame| through which it can connect MessagePorts to one
// or more services registered by the caller.
class NamedMessagePortConnector {
 public:
  using DefaultPortConnectedCallback = base::RepeatingCallback<void(
      base::StringPiece,
      fidl::InterfaceHandle<fuchsia::web::MessagePort>)>;

  // TODO(crbug.com/953958): Deprecated, remove this.
  using PortConnectedCallback = base::RepeatingCallback<void(
      fidl::InterfaceHandle<fuchsia::web::MessagePort>)>;

  explicit NamedMessagePortConnector(fuchsia::web::Frame* frame);
  ~NamedMessagePortConnector();

  // Sets the handler that is called for connected ports which aren't
  // registered in advance.
  // TODO(crbug.com/953958): Rename this to Register() when the transition is
  // complete.
  void RegisterDefaultHandler(DefaultPortConnectedCallback handler);

  // Registers a |handler| which will receive MessagePorts originating from
  // |frame_|'s web content. |port_name| is a non-empty, alphanumeric string
  // shared with the native backends.
  // TODO(crbug.com/953958): Remove this method.
  void Register(const std::string& port_name, PortConnectedCallback handler);

  // Unregisters a handler.
  // TODO(crbug.com/953958): Remove this method.
  void Unregister(const std::string& port_name);

  // Invoked by the caller after every |frame_| page load.
  // Half-connected ports from prior page generations will be discarded.
  void OnPageLoad();

 private:
  // Gets the next port from |control_port_|.
  void ReceiveNextConnectRequest();

  // Handles a port received from |control_port_|.
  void OnConnectRequest(fuchsia::web::WebMessage message);

  fuchsia::web::Frame* const frame_;

  // Invoked for ports which weren't previously Register()'ed.
  DefaultPortConnectedCallback default_handler_;

  // Deprecated.
  // TODO(crbug.com/953958): Remove this.
  std::map<std::string, PortConnectedCallback> port_connected_handlers_;

  fuchsia::web::MessagePortPtr control_port_;

  DISALLOW_COPY_AND_ASSIGN(NamedMessagePortConnector);
};

#endif  // FUCHSIA_RUNNERS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_H_
