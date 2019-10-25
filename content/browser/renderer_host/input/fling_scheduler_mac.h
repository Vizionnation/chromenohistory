// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_MAC_H_

#include "content/browser/renderer_host/input/fling_scheduler.h"

namespace content {

class RenderWidgetHostImpl;

class CONTENT_EXPORT FlingSchedulerMac : public FlingScheduler {
 public:
  FlingSchedulerMac(RenderWidgetHostImpl* host);
  ~FlingSchedulerMac() override;

 private:
  ui::Compositor* GetCompositor() override;

  DISALLOW_COPY_AND_ASSIGN(FlingSchedulerMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_MAC_H_
