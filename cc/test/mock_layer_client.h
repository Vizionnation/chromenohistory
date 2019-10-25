// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_MOCK_LAYER_CLIENT_H_
#define CC_TEST_MOCK_LAYER_CLIENT_H_

#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/traced_value.h"
#include "cc/layers/layer_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cc {

class MockLayerClient : public LayerClient {
 public:
  MockLayerClient();
  MockLayerClient(const MockLayerClient&) = delete;
  ~MockLayerClient() override;

  MockLayerClient& operator=(const MockLayerClient&) = delete;

  MOCK_METHOD1(TakeDebugInfo,
               std::unique_ptr<base::trace_event::TracedValue>(const Layer*));
  MOCK_METHOD1(didChangeScrollbarsHiddenIfOverlay, void(bool));
};

}  // namespace cc

#endif  // CC_TEST_MOCK_LAYER_CLIENT_H_
