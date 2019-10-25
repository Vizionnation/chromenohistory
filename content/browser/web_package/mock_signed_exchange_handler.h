// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_MOCK_SIGNED_EXCHANGE_HANDLER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_MOCK_SIGNED_EXCHANGE_HANDLER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/browser/web_package/signed_exchange_handler.h"
#include "net/base/hash_value.h"
#include "url/gurl.h"

namespace content {

class SignedExchangeCertFetcherFactory;

class MockSignedExchangeHandlerParams {
 public:
  // |mime_type| and |response_headers| are ignored if |error| is not net::OK.
  // If |signature_expire_time| is a null Time, we treat as one day after now.
  MockSignedExchangeHandlerParams(
      const GURL& outer_url,
      SignedExchangeLoadResult result,
      net::Error error,
      const GURL& inner_url,
      const std::string& mime_type,
      std::vector<std::string> response_headers,
      base::Optional<net::SHA256HashValue> header_integrity,
      const base::Time& signature_expire_time = base::Time());
  MockSignedExchangeHandlerParams(const MockSignedExchangeHandlerParams& other);
  ~MockSignedExchangeHandlerParams();
  const GURL outer_url;
  const SignedExchangeLoadResult result;
  const net::Error error;
  const GURL inner_url;
  const std::string mime_type;
  const std::vector<std::string> response_headers;
  const base::Optional<net::SHA256HashValue> header_integrity;
  const base::Time signature_expire_time;
};

class MockSignedExchangeHandler final : public SignedExchangeHandler {
 public:
  MockSignedExchangeHandler(const MockSignedExchangeHandlerParams& params,
                            std::unique_ptr<net::SourceStream> body,
                            ExchangeHeadersCallback headers_callback);
  ~MockSignedExchangeHandler() override;
  base::Optional<net::SHA256HashValue> ComputeHeaderIntegrity() const override;
  base::Time GetSignatureExpireTime() const override;

 private:
  const base::Optional<net::SHA256HashValue> header_integrity_;
  const base::Time signature_expire_time_;

  DISALLOW_COPY_AND_ASSIGN(MockSignedExchangeHandler);
};

class MockSignedExchangeHandlerFactory final
    : public SignedExchangeHandlerFactory {
 public:
  using ExchangeHeadersCallback =
      SignedExchangeHandler::ExchangeHeadersCallback;

  // Creates a factory that creates SignedExchangeHandler which fires
  // a headers callback with the matching MockSignedExchangeHandlerParams.
  MockSignedExchangeHandlerFactory(
      std::vector<MockSignedExchangeHandlerParams> params_list);
  ~MockSignedExchangeHandlerFactory() override;

  std::unique_ptr<SignedExchangeHandler> Create(
      const GURL& outer_url,
      std::unique_ptr<net::SourceStream> body,
      ExchangeHeadersCallback headers_callback,
      std::unique_ptr<SignedExchangeCertFetcherFactory> cert_fetcher_factory)
      override;

 private:
  const std::vector<MockSignedExchangeHandlerParams> params_list_;

  DISALLOW_COPY_AND_ASSIGN(MockSignedExchangeHandlerFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_MOCK_SIGNED_EXCHANGE_HANDLER_H_
