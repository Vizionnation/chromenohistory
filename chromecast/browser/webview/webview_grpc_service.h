// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_GRPC_SERVICE_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_GRPC_SERVICE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/browser/webview/proto/webview.grpc.pb.h"
#include "chromecast/browser/webview/webview_window_manager.h"
#include "third_party/grpc/src/include/grpcpp/server.h"

namespace chromecast {

// This is a service that provides a GRPC interface to create and control
// webviews. See the proto file for commands.
class WebviewAsyncService : public base::PlatformThread::Delegate {
 public:
  WebviewAsyncService(
      std::unique_ptr<webview::WebviewService::AsyncService> service,
      std::unique_ptr<grpc::ServerCompletionQueue> cq,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~WebviewAsyncService() override;

 private:
  void ThreadMain() override;

  // Separate thread to run the gRPC completion queue on.
  base::PlatformThreadHandle rpc_thread_;

  // Requests need to be posted back to the browser main UI thread to manage
  // Webview state.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  std::unique_ptr<webview::WebviewService::AsyncService> service_;

  WebviewWindowManager window_manager_;

  DISALLOW_COPY_AND_ASSIGN(WebviewAsyncService);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_GRPC_SERVICE_H_
