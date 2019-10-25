// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/request_action.h"

namespace extensions {
namespace declarative_net_request {

RequestAction::RequestAction(
    RequestAction::Type type,
    int rule_id,
    api::declarative_net_request::SourceType source_type,
    const ExtensionId& extension_id)
    : type(type),
      rule_id(rule_id),
      source_type(source_type),
      extension_id(extension_id) {}
RequestAction::~RequestAction() = default;
RequestAction::RequestAction(RequestAction&&) = default;
RequestAction& RequestAction::operator=(RequestAction&&) = default;

}  // namespace declarative_net_request
}  // namespace extensions
