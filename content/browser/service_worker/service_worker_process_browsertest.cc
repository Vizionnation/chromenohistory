// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"

// This file has tests involving render process selection for service workers.

namespace content {

class ServiceWorkerProcessBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ServiceWorkerProcessBrowserTest() = default;
  ~ServiceWorkerProcessBrowserTest() override = default;

  ServiceWorkerProcessBrowserTest(const ServiceWorkerProcessBrowserTest&) =
      delete;
  ServiceWorkerProcessBrowserTest& operator=(
      const ServiceWorkerProcessBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    StoragePartition* partition = BrowserContext::GetDefaultStoragePartition(
        shell()->web_contents()->GetBrowserContext());
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (SitePerProcess())
      command_line->AppendSwitch(switches::kSitePerProcess);
  }

 protected:
  bool SitePerProcess() const { return GetParam(); }

  // Registers a service worker and then tears down the process it used, for a
  // clean slate going forward.
  void RegisterServiceWorker() {
    // Load a page that registers a service worker.
    Shell* start_shell = CreateBrowser();
    ASSERT_TRUE(NavigateToURL(
        start_shell, embedded_test_server()->GetURL(
                         "/service_worker/create_service_worker.html")));
    ASSERT_EQ("DONE",
              EvalJs(start_shell, "register('fetch_event_pass_through.js');"));

    auto* host = RenderProcessHost::FromID(GetServiceWorkerProcessId());
    ASSERT_TRUE(host);
    RenderProcessHostWatcher exit_watcher(
        host, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

    // Tear down the page.
    start_shell->Close();

    // Stop the service worker. The process should exit.
    base::RunLoop loop;
    wrapper()->StopAllServiceWorkers(loop.QuitClosure());
    loop.Run();
    exit_watcher.Wait();
  }

  // Returns the number of running service workers.
  size_t GetRunningServiceWorkerCount() {
    return wrapper()->GetRunningServiceWorkerInfos().size();
  }

  // Returns the process id of the running service worker. There must be exactly
  // one service worker running.
  int GetServiceWorkerProcessId() {
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        wrapper()->GetRunningServiceWorkerInfos();
    DCHECK_EQ(infos.size(), 1u);
    const ServiceWorkerRunningInfo& info = infos.begin()->second;
    return info.render_process_id;
  }

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetFrameTree()->root()->current_frame_host();
  }

 private:
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
};

// Tests that a service worker started due to a navigation shares the same
// process as the navigation.
IN_PROC_BROWSER_TEST_P(ServiceWorkerProcessBrowserTest,
                       ServiceWorkerAndPageShareProcess) {
  // Register the service worker.
  RegisterServiceWorker();

  // Navigate to a page in the service worker's scope.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/empty.html")));

  // The page and service worker should be in the same process.
  int page_process_id = current_frame_host()->GetProcess()->GetID();
  EXPECT_NE(page_process_id, ChildProcessHost::kInvalidUniqueID);
  ASSERT_EQ(GetRunningServiceWorkerCount(), 1u);
  int worker_process_id = GetServiceWorkerProcessId();
  EXPECT_EQ(page_process_id, worker_process_id);
}

// ContentBrowserClient that skips assigning a site URL for a given URL.
class DontAssignSiteContentBrowserClient : public TestContentBrowserClient {
 public:
  // Any visit to |url_to_skip| will not cause the site to be assigned to the
  // SiteInstance.
  explicit DontAssignSiteContentBrowserClient(const GURL& url_to_skip)
      : url_to_skip_(url_to_skip) {}

  DontAssignSiteContentBrowserClient(
      const DontAssignSiteContentBrowserClient&) = delete;
  DontAssignSiteContentBrowserClient& operator=(
      const DontAssignSiteContentBrowserClient&) = delete;

  bool ShouldAssignSiteForURL(const GURL& url) override {
    return url == url_to_skip_;
  }

 private:
  GURL url_to_skip_;
};

// Tests that a service worker and navigation share the same process in the
// special case where the service worker starts before the navigation starts,
// and the navigation transitions out of a page with no site URL. This special
// case happens in real life when doing a search from the omnibox while on the
// Android native NTP page: the service worker starts first due to the
// navigation hint from the omnibox, and the native page has no site URL. See
// https://crbug.com/1012143.
IN_PROC_BROWSER_TEST_P(
    ServiceWorkerProcessBrowserTest,
    ServiceWorkerAndPageShareProcess_NavigateFromUnassignedSiteInstance) {
  // Set up a page URL that will have no site URL.
  GURL empty_site = embedded_test_server()->GetURL("a.com", "/title1.html");
  DontAssignSiteContentBrowserClient content_browser_client(empty_site);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);

  // Register the service worker.
  RegisterServiceWorker();

  // Navigate to the empty site instance page. Subsequent navigations from
  // this page will prefer to use the same process by default.
  ASSERT_TRUE(NavigateToURL(shell(), empty_site));

  // Start the service worker. It will start in a new process.
  base::RunLoop loop;
  GURL scope = embedded_test_server()->GetURL("/service_worker/");
  wrapper()->StartWorkerForScope(
      scope,
      base::BindLambdaForTesting([&loop](int64_t version_id, int process_id,
                                         int thread_id) { loop.Quit(); }),
      base::BindLambdaForTesting([&loop]() {
        ASSERT_FALSE(true) << "start worker failed";
        loop.Quit();
      }));
  loop.Run();

  // Navigate to a page in the service worker's scope.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/empty.html")));

  // The page and service worker should be in the same process.
  ASSERT_EQ(GetRunningServiceWorkerCount(), 1u);
  int page_process_id = current_frame_host()->GetProcess()->GetID();
  EXPECT_NE(page_process_id, ChildProcessHost::kInvalidUniqueID);
  EXPECT_EQ(page_process_id, GetServiceWorkerProcessId());

  SetBrowserClientForTesting(old_client);
}

// Toggle Site Isolation.
INSTANTIATE_TEST_SUITE_P(, ServiceWorkerProcessBrowserTest, testing::Bool());

}  // namespace content
