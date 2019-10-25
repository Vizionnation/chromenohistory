// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_response_validator.h"

#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"

namespace extensions {

namespace {

APIResponseValidator::TestHandler::HandlerMethod*
    g_failure_handler_for_testing = nullptr;

}  // namespace

APIResponseValidator::TestHandler::TestHandler(HandlerMethod method)
    : method_(method) {
  DCHECK(!g_failure_handler_for_testing)
      << "Only one TestHandler is allowed at a time.";
  g_failure_handler_for_testing = &method_;
}

APIResponseValidator::TestHandler::~TestHandler() {
  DCHECK_EQ(&method_, g_failure_handler_for_testing);
  g_failure_handler_for_testing = nullptr;
}

APIResponseValidator::APIResponseValidator(const APITypeReferenceMap* type_refs)
    : type_refs_(type_refs) {}

APIResponseValidator::~APIResponseValidator() = default;

void APIResponseValidator::ValidateResponse(
    v8::Local<v8::Context> context,
    const std::string& method_name,
    const std::vector<v8::Local<v8::Value>> response_arguments,
    const std::string& api_error,
    CallbackType callback_type) {
  DCHECK(binding::IsResponseValidationEnabled());

  // If the callback is API-provided, the response can't be validated against
  // the expected schema because the callback may modify the arguments.
  if (callback_type == CallbackType::kAPIProvided)
    return;

  // If the call failed, there are no expected arguments.
  if (!api_error.empty()) {
    // TODO(devlin): It would be really nice to validate that
    // |response_arguments| is empty here, but some functions both set an error
    // and supply arguments.
    return;
  }

  const APISignature* signature = type_refs_->GetCallbackSignature(method_name);
  // If there's no corresponding signature, don't validate. This can
  // legitimately happen with APIs that create custom requests.
  if (!signature)
    return;

  std::string error;
  if (signature->ValidateResponse(context, response_arguments, *type_refs_,
                                  &error)) {
    // Response was valid.
    return;
  }

  // The response did not match the expected schema.
  if (g_failure_handler_for_testing) {
    g_failure_handler_for_testing->Run(method_name, error);
  } else {
    NOTREACHED() << "Error validating response to `" << method_name
                 << "`: " << error;
  }
}

}  // namespace extensions
