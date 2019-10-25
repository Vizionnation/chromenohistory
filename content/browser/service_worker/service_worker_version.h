// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/id_map.h"
#include "base/debug/stack_trace.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_client_info.h"
#include "content/browser/service_worker/service_worker_client_utils.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_ping_controller.h"
#include "content/browser/service_worker/service_worker_script_cache_map.h"
#include "content/browser/service_worker/service_worker_update_checker.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class HttpResponseInfo;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerInstalledScriptsSender;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
struct ServiceWorkerVersionInfo;

namespace service_worker_controllee_request_handler_unittest {
class ServiceWorkerControlleeRequestHandlerTest;
FORWARD_DECLARE_TEST(ServiceWorkerControlleeRequestHandlerTest,
                     ActivateWaitingVersion);
FORWARD_DECLARE_TEST(ServiceWorkerControlleeRequestHandlerTest,
                     FallbackWithNoFetchHandler);
}  // namespace service_worker_controllee_request_handler_unittest

namespace service_worker_version_unittest {
class ServiceWorkerVersionTest;
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, FailToStart_Timeout);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, IdleTimeout);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, MixedRequestTimeouts);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RegisterForeignFetchScopes);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RequestCustomizedTimeout);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RequestNowTimeout);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RequestTimeout);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RestartWorker);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RequestNowTimeoutKill);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, SetDevToolsAttached);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StaleUpdate_DoNotDeferTimer);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StaleUpdate_FreshWorker);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StaleUpdate_NonActiveWorker);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StaleUpdate_RunningWorker);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StaleUpdate_StartWorker);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest,
                     StallInStopping_DetachThenRestart);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StallInStopping_DetachThenStart);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StartRequestWithNullContext);
}  // namespace service_worker_version_unittest

namespace service_worker_storage_unittest {
FORWARD_DECLARE_TEST(ServiceWorkerStorageDiskTest, ScriptResponseTime);
}  // namespace service_worker_storage_unittest

namespace service_worker_registration_unittest {
class ServiceWorkerActivationTest;
}  // namespace service_worker_registration_unittest

namespace service_worker_navigation_loader_unittest {
class ServiceWorkerNavigationLoaderTest;
}  // namespace service_worker_navigation_loader_unittest

// This class corresponds to a specific version of a ServiceWorker
// script for a given scope. When a script is upgraded, there may be
// more than one ServiceWorkerVersion "running" at a time, but only
// one of them is activated. This class connects the actual script with a
// running worker.
//
// Unless otherwise noted, all methods of this class run on the IO thread.
class CONTENT_EXPORT ServiceWorkerVersion
    : public blink::mojom::ServiceWorkerHost,
      public base::RefCounted<ServiceWorkerVersion>,
      public EmbeddedWorkerInstance::Listener {
 public:
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)>;
  using SimpleEventCallback =
      base::OnceCallback<void(blink::mojom::ServiceWorkerEventStatus)>;
  using FetchHandlerExistence = blink::mojom::FetchHandlerExistence;

  // Current version status; some of the status (e.g. INSTALLED and ACTIVATED)
  // should be persisted unlike running status.
  enum Status {
    NEW,         // The version is just created.
    INSTALLING,  // Install event is dispatched and being handled.
    INSTALLED,   // Install event is finished and is ready to be activated.
    ACTIVATING,  // Activate event is dispatched and being handled.
    ACTIVATED,   // Activation is finished and can run as activated.
    REDUNDANT,   // The version is no longer running as activated, due to
                 // unregistration or replace.
  };

  // Behavior when a request times out.
  enum TimeoutBehavior {
    KILL_ON_TIMEOUT,     // Kill the worker if this request times out.
    CONTINUE_ON_TIMEOUT  // Keep the worker alive, only abandon the request that
                         // timed out.
  };

  class Observer {
   public:
    virtual void OnRunningStateChanged(ServiceWorkerVersion* version) {}
    virtual void OnVersionStateChanged(ServiceWorkerVersion* version) {}
    virtual void OnDevToolsRoutingIdChanged(ServiceWorkerVersion* version) {}
    virtual void OnMainScriptHttpResponseInfoSet(
        ServiceWorkerVersion* version) {}
    virtual void OnErrorReported(ServiceWorkerVersion* version,
                                 const base::string16& error_message,
                                 int line_number,
                                 int column_number,
                                 const GURL& source_url) {}
    virtual void OnReportConsoleMessage(
        ServiceWorkerVersion* version,
        blink::mojom::ConsoleMessageSource source,
        blink::mojom::ConsoleMessageLevel message_level,
        const base::string16& message,
        int line_number,
        const GURL& source_url) {}
    // OnControlleeAdded/Removed are called asynchronously. It is possible the
    // provider host identified by |client_uuid| was already destroyed when they
    // are called.
    virtual void OnControlleeAdded(ServiceWorkerVersion* version,
                                   const std::string& client_uuid,
                                   const ServiceWorkerClientInfo& client_info) {
    }
    virtual void OnControlleeRemoved(ServiceWorkerVersion* version,
                                     const std::string& client_uuid) {}
    virtual void OnNoControllees(ServiceWorkerVersion* version) {}
    virtual void OnNoWork(ServiceWorkerVersion* version) {}
    virtual void OnCachedMetadataUpdated(ServiceWorkerVersion* version,
                                         size_t size) {}

   protected:
    virtual ~Observer() {}
  };

  ServiceWorkerVersion(ServiceWorkerRegistration* registration,
                       const GURL& script_url,
                       blink::mojom::ScriptType script_type,
                       int64_t version_id,
                       base::WeakPtr<ServiceWorkerContextCore> context);

  int64_t version_id() const { return version_id_; }
  int64_t registration_id() const { return registration_id_; }
  const GURL& script_url() const { return script_url_; }
  const url::Origin& script_origin() const { return script_origin_; }
  const GURL& scope() const { return scope_; }
  blink::mojom::ScriptType script_type() const { return script_type_; }
  EmbeddedWorkerStatus running_status() const {
    return embedded_worker_->status();
  }
  ServiceWorkerVersionInfo GetInfo();
  Status status() const { return status_; }

  // This status is set to EXISTS or DOES_NOT_EXIST when the install event has
  // been executed in a new version or when an installed version is loaded from
  // the storage. When a new version is not installed yet, it is UNKNOWN.
  FetchHandlerExistence fetch_handler_existence() const {
    return fetch_handler_existence_;
  }
  // This also updates |site_for_uma_| when it was Site::OTHER.
  void set_fetch_handler_existence(FetchHandlerExistence existence);

  base::TimeDelta TimeSinceNoControllees() const {
    return GetTickDuration(no_controllees_time_);
  }

  base::TimeDelta TimeSinceSkipWaiting() const {
    return GetTickDuration(skip_waiting_time_);
  }

  // Meaningful only if this version is active.
  const blink::mojom::NavigationPreloadState& navigation_preload_state() const {
    DCHECK(status_ == ACTIVATING || status_ == ACTIVATED) << status_;
    return navigation_preload_state_;
  }
  // Only intended for use by ServiceWorkerRegistration. Generally use
  // ServiceWorkerRegistration::EnableNavigationPreload or
  // ServiceWorkerRegistration::SetNavigationPreloadHeader instead of this
  // function.
  void SetNavigationPreloadState(
      const blink::mojom::NavigationPreloadState& state);

  ServiceWorkerMetrics::Site site_for_uma() const { return site_for_uma_; }

  // This sets the new status and also run status change callbacks
  // if there're any (see RegisterStatusChangeCallback).
  void SetStatus(Status status);

  // Registers status change callback. (This is for one-off observation,
  // the consumer needs to re-register if it wants to continue observing
  // status changes)
  void RegisterStatusChangeCallback(base::OnceClosure callback);

  // Starts an embedded worker for this version.
  // This returns OK (success) if the worker is already running.
  // |purpose| is recorded in UMA.
  void StartWorker(ServiceWorkerMetrics::EventType purpose,
                   StatusCallback callback);

  // Stops an embedded worker for this version.
  void StopWorker(base::OnceClosure callback);

  // Asks the renderer to notify the browser that it becomes idle as soon as
  // possible, and it results in letting idle termination occur earlier. This is
  // typically used for activation. An active worker needs to be swapped out
  // soon after the service worker becomes idle if a waiting worker exists.
  void TriggerIdleTerminationAsap();

  // Called when the renderer notifies the browser that the worker is now idle.
  // Returns true if the worker will be terminated and the worker should not
  // handle any events dispatched directly from clients (e.g. FetchEvents for
  // subresources).
  bool OnRequestTermination();

  // Skips waiting and forces this version to become activated.
  void SkipWaitingFromDevTools();

  // Schedules an update to be run 'soon'.
  void ScheduleUpdate();

  // Starts an update now.
  void StartUpdate();

  // Starts the worker if it isn't already running. Calls |callback| with
  // blink::ServiceWorkerStatusCode::kOk when the worker started
  // up successfully or if it is already running. Otherwise, calls |callback|
  // with an error code. If the worker is already running, |callback| is
  // executed synchronously (before this method returns). |purpose| is used for
  // UMA.
  void RunAfterStartWorker(ServiceWorkerMetrics::EventType purpose,
                           StatusCallback callback);

  // Call this while the worker is running before dispatching an event to the
  // worker. This informs ServiceWorkerVersion about the event in progress. The
  // worker attempts to keep running until the event finishes.
  //
  // Returns a request id, which must later be passed to FinishRequest when the
  // event finished. The caller is responsible for ensuring FinishRequest is
  // called. If FinishRequest is not called the request will eventually time
  // out and the worker will be forcibly terminated.
  //
  // The |error_callback| is called if either ServiceWorkerVersion decides the
  // event is taking too long, or if for some reason the worker stops or is
  // killed before the request finishes. In this case, the caller should not
  // call FinishRequest.
  int StartRequest(ServiceWorkerMetrics::EventType event_type,
                   StatusCallback error_callback);

  // Same as StartRequest, but allows the caller to specify a custom timeout for
  // the event, as well as the behavior for when the request times out.
  //
  // S13nServiceWorker: |timeout| and |timeout_behavior| don't have any effect.
  // They are just ignored. Timeouts can be added to the
  // blink::mojom::ServiceWorker interface instead (see DispatchSyncEvent for an
  // example).
  int StartRequestWithCustomTimeout(ServiceWorkerMetrics::EventType event_type,
                                    StatusCallback error_callback,
                                    const base::TimeDelta& timeout,
                                    TimeoutBehavior timeout_behavior);

  // Starts a request of type EventType::EXTERNAL_REQUEST.
  // Provides a mechanism to external clients to keep the worker running.
  // |request_uuid| is a GUID for clients to identify the request.
  // Returns true if the request was successfully scheduled to starrt.
  ServiceWorkerExternalRequestResult StartExternalRequest(
      const std::string& request_uuid);

  // Informs ServiceWorkerVersion that an event has finished being dispatched.
  // Returns false if no inflight requests with the provided id exist, for
  // example if the request has already timed out.
  // Pass the result of the event to |was_handled|, which is used to record
  // statistics based on the event status.
  // TODO(mek): Use something other than a bool for event status.
  bool FinishRequest(int request_id, bool was_handled);

  // Finishes an external request that was started by StartExternalRequest().
  ServiceWorkerExternalRequestResult FinishExternalRequest(
      const std::string& request_uuid);

  // Creates a callback that is to be used for marking simple events dispatched
  // through blink::mojom::ServiceWorker as finished for the |request_id|.
  // Simple event means those events expecting a response with only a status
  // code and the dispatch time. See service_worker.mojom.
  SimpleEventCallback CreateSimpleEventCallback(int request_id);

  // This must be called when the worker is running.
  blink::mojom::ServiceWorker* endpoint() {
    DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
           running_status() == EmbeddedWorkerStatus::RUNNING);
    DCHECK(service_worker_remote_.is_bound());
    return service_worker_remote_.get();
  }

  // Returns the 'controller' interface ptr of this worker. It is expected that
  // the worker is already starting or running, or is going to be started soon.
  // TODO(kinuko): Relying on the callsites to start the worker when it's
  // not running is a bit sketchy, maybe this should queue a task to check
  // if the pending request is pending too long? https://crbug.com/797222
  blink::mojom::ControllerServiceWorker* controller() {
    if (!remote_controller_.is_bound()) {
      DCHECK(!controller_receiver_.is_valid());
      controller_receiver_ = remote_controller_.BindNewPipeAndPassReceiver();
    }
    return remote_controller_.get();
  }

  // Adds and removes the specified host as a controllee of this service worker.
  void AddControllee(ServiceWorkerProviderHost* provider_host);
  void RemoveControllee(const std::string& client_uuid);

  // Returns if it has controllee.
  bool HasControllee() const { return !controllee_map_.empty(); }
  std::map<std::string, ServiceWorkerProviderHost*> controllee_map() {
    return controllee_map_;
  }

  // The provider host hosting this version. Only valid while the version is
  // running.
  ServiceWorkerProviderHost* provider_host() {
    DCHECK(provider_host_);
    return provider_host_.get();
  }

  base::WeakPtr<ServiceWorkerContextCore> context() const { return context_; }

  // Adds and removes Observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  ServiceWorkerScriptCacheMap* script_cache_map() { return &script_cache_map_; }
  EmbeddedWorkerInstance* embedded_worker() { return embedded_worker_.get(); }

  // Reports the error message to |observers_|.
  void ReportError(blink::ServiceWorkerStatusCode status,
                   const std::string& status_message);

  void ReportForceUpdateToDevTools();

  // Sets the status code to pass to StartWorker callbacks if start fails.
  void SetStartWorkerStatusCode(blink::ServiceWorkerStatusCode status);

  // Sets this version's status to REDUNDANT and deletes its resources.
  void Doom();
  bool is_redundant() const { return status_ == REDUNDANT; }

  bool skip_waiting() const { return skip_waiting_; }
  void set_skip_waiting(bool skip_waiting) { skip_waiting_ = skip_waiting; }

  bool skip_recording_startup_time() const {
    return skip_recording_startup_time_;
  }

  bool force_bypass_cache_for_scripts() const {
    return force_bypass_cache_for_scripts_;
  }
  void set_force_bypass_cache_for_scripts(bool force_bypass_cache_for_scripts) {
    force_bypass_cache_for_scripts_ = force_bypass_cache_for_scripts;
  }

  // Used for pausing service worker startup in the renderer in order to do the
  // byte-for-byte check.
  bool pause_after_download() const {
    return !pause_after_download_callback_.is_null();
  }
  void SetToPauseAfterDownload(base::OnceClosure callback);
  void SetToNotPauseAfterDownload();

  // For use by EmbeddedWorkerInstance. Called when the main script loaded.
  // This is only called for new (non-installed) workers. It's used for resuming
  // a paused worker via ResumeAfterDownload().
  void OnMainScriptLoaded();

  // Returns nullptr if the main script is not loaded yet and:
  //  1) The worker is a new one.
  //  OR
  //  2) The worker is an existing one but the entry in ServiceWorkerDatabase
  //     was written by old version of Chrome (< M56), so |origin_trial_tokens|
  //     wasn't set in the entry.
  const blink::TrialTokenValidator::FeatureToTokensMap* origin_trial_tokens()
      const {
    return origin_trial_tokens_.get();
  }
  // Set valid tokens in |tokens|. Invalid tokens in |tokens| are ignored.
  void SetValidOriginTrialTokens(
      const blink::TrialTokenValidator::FeatureToTokensMap& tokens);

  void SetDevToolsAttached(bool attached);

  // Sets the HttpResponseInfo used to load the main script.
  // This HttpResponseInfo will be used for all responses sent back from the
  // service worker, as the effective security of these responses is equivalent
  // to that of the ServiceWorker.
  void SetMainScriptHttpResponseInfo(const net::HttpResponseInfo& http_info);
  const net::HttpResponseInfo* GetMainScriptHttpResponseInfo();

  // Simulate ping timeout. Should be used for tests-only.
  void SimulatePingTimeoutForTesting();

  // Used to allow tests to change time for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // Used to allow tests to change wall clock for testing.
  void SetClockForTesting(base::Clock* clock);

  // Returns true when the service worker isn't handling any events or stream
  // responses, initiated from either the browser or the renderer.
  bool HasNoWork() const;

  // Returns the number of pending external request count of this worker.
  size_t GetExternalRequestCountForTest() const {
    return external_request_uuid_to_request_id_.size();
  }

  // Returns the amount of time left until the request with the latest
  // expiration time expires.
  base::TimeDelta remaining_timeout() const {
    return max_request_expiration_time_ - tick_clock_->NowTicks();
  }

  void CountFeature(blink::mojom::WebFeature feature);
  void set_used_features(std::set<blink::mojom::WebFeature> used_features) {
    used_features_ = std::move(used_features);
  }
  const std::set<blink::mojom::WebFeature>& used_features() const {
    return used_features_;
  }

  void set_script_response_time_for_devtools(base::Time response_time) {
    script_response_time_for_devtools_ = std::move(response_time);
  }

  static bool IsInstalled(ServiceWorkerVersion::Status status);
  static std::string VersionStatusToString(ServiceWorkerVersion::Status status);

  // For scheduling Soft Update after main resource requests. We schedule
  // a Soft Update to happen "soon" after each main resource request, attempting
  // to do the update after the page load finished. The renderer sends a hint
  // when it's a good time to update. This is a count of outstanding expected
  // hints, to handle multiple main resource requests occurring near the same
  // time.
  //
  // On each request that dispatches a fetch event to this worker (or would
  // have, in the case of a no-fetch event worker), this count is incremented.
  // When the browser-side provider host receives a hint from the renderer that
  // it is a good time to update the service worker, the count is decremented.
  // It is also decremented when if the provider host is destroyed before
  // receiving the hint.
  //
  // When the count transitions from 1 to 0, update is scheduled.
  void IncrementPendingUpdateHintCount();
  void DecrementPendingUpdateHintCount();

  // ServiceWorkerImportedScriptUpdateCheck:
  // Called on versions created for an update check. Called if the check
  // determined an update exists before starting the worker for an install
  // event.
  void PrepareForUpdate(
      std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>
          compared_script_info_map,
      const GURL& updated_script_url);
  const std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>&
  compared_script_info_map() const;
  ServiceWorkerUpdateChecker::ComparedScriptInfo TakeComparedScriptInfo(
      const GURL& script_url);

  // Called by the EmbeddedWorkerInstance to determine if its worker process
  // should be kept at foreground priority.
  bool ShouldRequireForegroundPriority(int worker_process_id) const;

  // Called when a controlled client's state changes in a way that might effect
  // whether the service worker should be kept at foreground priority.
  void UpdateForegroundPriority();

  // Adds a message to the service worker's DevTools console.
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message);

  // Adds a message to service worker internals UI page if the internal page is
  // opened. Use this method only for events which can't be logged on the
  // worker's DevTools console, e.g., the worker is not responding. For regular
  // events use AddMessageToConsole().
  void MaybeReportConsoleMessageToInternals(
      blink::mojom::ConsoleMessageLevel message_level,
      const std::string& message);

  // TODO(crbug.com/951571): Remove once the bug is debugged.
  const base::debug::StackTrace& redundant_state_callstack() const {
    return redundant_state_callstack_;
  }

  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerHost>&
  service_worker_host_receiver_for_testing() {
    return receiver_;
  }

 private:
  friend class base::RefCounted<ServiceWorkerVersion>;
  friend class EmbeddedWorkerInstanceTest;
  friend class ServiceWorkerPingController;
  friend class ServiceWorkerProviderHostTest;
  friend class ServiceWorkerReadFromCacheJobTest;
  friend class ServiceWorkerVersionBrowserTest;
  friend class ServiceWorkerActivationTest;
  friend class service_worker_version_unittest::ServiceWorkerVersionTest;
  friend class service_worker_navigation_loader_unittest::
      ServiceWorkerNavigationLoaderTest;

  FRIEND_TEST_ALL_PREFIXES(service_worker_controllee_request_handler_unittest::
                               ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);
  FRIEND_TEST_ALL_PREFIXES(service_worker_controllee_request_handler_unittest::
                               ServiceWorkerControlleeRequestHandlerTest,
                           FallbackWithNoFetchHandler);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProviderHostTest,
                           DontSetControllerInDestructor);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, Register);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      IdleTimeout);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      SetDevToolsAttached);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StaleUpdate_FreshWorker);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StaleUpdate_NonActiveWorker);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StaleUpdate_StartWorker);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StaleUpdate_RunningWorker);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StaleUpdate_DoNotDeferTimer);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StartRequestWithNullContext);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      FailToStart_Timeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutStartingWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutWorkerInEvent);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StallInStopping_DetachThenStart);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StallInStopping_DetachThenRestart);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      RequestNowTimeout);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      RequestTimeout);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      RestartWorker);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      RequestNowTimeoutKill);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      RequestCustomizedTimeout);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      MixedRequestTimeouts);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_storage_unittest::ServiceWorkerStorageDiskTest,
      ScriptResponseTime);

  // Contains timeout info for InflightRequest.
  struct InflightRequestTimeoutInfo {
    InflightRequestTimeoutInfo(int id,
                               ServiceWorkerMetrics::EventType event_type,
                               const base::TimeTicks& expiration,
                               TimeoutBehavior timeout_behavior);
    ~InflightRequestTimeoutInfo();
    // Compares |expiration|, or |id| if |expiration| is the same.
    bool operator<(const InflightRequestTimeoutInfo& other) const;

    const int id;
    const ServiceWorkerMetrics::EventType event_type;
    const base::TimeTicks expiration;
    const TimeoutBehavior timeout_behavior;
  };

  // Keeps track of the status of each request, which starts at StartRequest()
  // and ends at FinishRequest().
  struct InflightRequest {
    InflightRequest(StatusCallback error_callback,
                    base::Time time,
                    const base::TimeTicks& time_ticks,
                    ServiceWorkerMetrics::EventType event_type);
    ~InflightRequest();

    StatusCallback error_callback;
    base::Time start_time;
    base::TimeTicks start_time_ticks;
    ServiceWorkerMetrics::EventType event_type;
    // Points to this request's entry in |request_timeouts_|.
    std::set<InflightRequestTimeoutInfo>::iterator timeout_iter;
  };

  // The timeout timer interval.
  static constexpr base::TimeDelta kTimeoutTimerDelay =
      base::TimeDelta::FromSeconds(30);
  // Timeout for a new worker to start.
  static constexpr base::TimeDelta kStartNewWorkerTimeout =
      base::TimeDelta::FromMinutes(5);
  // Timeout for the worker to stop.
  static constexpr base::TimeDelta kStopWorkerTimeout =
      base::TimeDelta::FromSeconds(5);

  ~ServiceWorkerVersion() override;

  // The following methods all rely on the internal |tick_clock_| for the
  // current time.
  void RestartTick(base::TimeTicks* time) const;
  bool RequestExpired(const base::TimeTicks& expiration) const;
  base::TimeDelta GetTickDuration(const base::TimeTicks& time) const;

  // EmbeddedWorkerInstance::Listener overrides:
  void OnScriptEvaluationStart() override;
  void OnStarting() override;
  void OnStarted(blink::mojom::ServiceWorkerStartStatus status) override;
  void OnStopping() override;
  void OnStopped(EmbeddedWorkerStatus old_status) override;
  void OnDetached(EmbeddedWorkerStatus old_status) override;
  void OnRegisteredToDevToolsManager() override;
  void OnReportException(const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url) override;
  void OnReportConsoleMessage(blink::mojom::ConsoleMessageSource source,
                              blink::mojom::ConsoleMessageLevel message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override;

  void OnStartSent(blink::ServiceWorkerStatusCode status);

  // Implements blink::mojom::ServiceWorkerHost.
  void SetCachedMetadata(const GURL& url,
                         base::span<const uint8_t> data) override;
  void ClearCachedMetadata(const GURL& url) override;
  void ClaimClients(ClaimClientsCallback callback) override;
  void GetClients(blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
                  GetClientsCallback callback) override;
  void GetClient(const std::string& client_uuid,
                 GetClientCallback callback) override;
  void GetClientInternal(const std::string& client_uuid,
                         GetClientCallback callback);
  void OpenNewTab(const GURL& url, OpenNewTabCallback callback) override;
  void OpenPaymentHandlerWindow(
      const GURL& url,
      OpenPaymentHandlerWindowCallback callback) override;
  void PostMessageToClient(const std::string& client_uuid,
                           blink::TransferableMessage message) override;
  void FocusClient(const std::string& client_uuid,
                   FocusClientCallback callback) override;
  void NavigateClient(const std::string& client_uuid,
                      const GURL& url,
                      NavigateClientCallback callback) override;
  void SkipWaiting(SkipWaitingCallback callback) override;

  void OnSetCachedMetadataFinished(int64_t callback_id,
                                   size_t size,
                                   int result);
  void OnClearCachedMetadataFinished(int64_t callback_id, int result);
  void OpenWindow(GURL url,
                  service_worker_client_utils::WindowType type,
                  OpenNewTabCallback callback);

  void OnPongFromWorker();

  void DidEnsureLiveRegistrationForStartWorker(
      ServiceWorkerMetrics::EventType purpose,
      Status prestart_status,
      bool is_browser_startup_complete,
      StatusCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void StartWorkerInternal();

  // Stops the worker if it is idle (has no in-flight requests) or timed out
  // ping.
  void StopWorkerIfIdle();

  // Returns true if the service worker is known to have work to do because the
  // browser process initiated a request to the service worker which isn't done
  // yet.
  //
  // Note that this method may return false even when the service worker still
  // has work to do; clients may dispatch events to the service worker directly.
  // You can ensure no inflight requests exist when HasWorkInBrowser() returns
  // false and |worker_is_idle_on_renderer_| is true, or when the worker is
  // stopped.
  bool HasWorkInBrowser() const;

  // Callback function for simple events dispatched through mojo interface
  // blink::mojom::ServiceWorker. Use CreateSimpleEventCallback() to
  // create a callback for a given |request_id|.
  void OnSimpleEventFinished(int request_id,
                             blink::mojom::ServiceWorkerEventStatus status);

  // The timeout timer periodically calls OnTimeoutTimer, which stops the worker
  // if it is excessively idle or unresponsive to ping.
  void StartTimeoutTimer();
  void StopTimeoutTimer();
  void OnTimeoutTimer();
  void SetTimeoutTimerInterval(base::TimeDelta interval);

  // Called by ServiceWorkerPingController for ping protocol.
  void PingWorker();
  void OnPingTimeout();

  // RecordStartWorkerResult is added as a start callback by StartTimeoutTimer
  // and records metrics about startup.
  void RecordStartWorkerResult(ServiceWorkerMetrics::EventType purpose,
                               Status prestart_status,
                               int trace_id,
                               bool is_browser_startup_complete,
                               blink::ServiceWorkerStatusCode status);

  bool MaybeTimeoutRequest(const InflightRequestTimeoutInfo& info);
  void SetAllRequestExpirations(const base::TimeTicks& expiration);

  // Returns the reason the embedded worker failed to start, using information
  // inaccessible to EmbeddedWorkerInstance. Returns |default_code| if it can't
  // deduce a reason.
  blink::ServiceWorkerStatusCode DeduceStartWorkerFailureReason(
      blink::ServiceWorkerStatusCode default_code);

  // Sets |stale_time_| if this worker is stale, causing an update to eventually
  // occur once the worker stops or is running too long.
  void MarkIfStale();

  void FoundRegistrationForUpdate(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void OnStoppedInternal(EmbeddedWorkerStatus old_status);

  // Fires and clears all start callbacks.
  void FinishStartWorker(blink::ServiceWorkerStatusCode status);

  // Removes any pending external request that has GUID of |request_uuid|.
  void CleanUpExternalRequest(const std::string& request_uuid,
                              blink::ServiceWorkerStatusCode status);

  // Called if no inflight events exist on the browser process. Triggers
  // OnNoWork() if the renderer-side idle timeout has been fired or the worker
  // has been stopped.
  void OnNoWorkInBrowser();

  bool IsStartWorkerAllowed() const;

  void NotifyControlleeAdded(const std::string& uuid,
                             const ServiceWorkerClientInfo& info);
  void NotifyControlleeRemoved(const std::string& uuid);

  void GetClientOnExecutionReady(const std::string& client_uuid,
                                 GetClientCallback callback,
                                 bool success);

  void InitializeGlobalScope();

  const int64_t version_id_;
  const int64_t registration_id_;
  const GURL script_url_;
  const url::Origin script_origin_;
  const GURL scope_;
  // A service worker has an associated type which is either
  // "classic" or "module". Unless stated otherwise, it is "classic".
  // https://w3c.github.io/ServiceWorker/#dfn-type
  const blink::mojom::ScriptType script_type_;
  FetchHandlerExistence fetch_handler_existence_;
  // The source of truth for navigation preload state is the
  // ServiceWorkerRegistration. |navigation_preload_state_| is essentially a
  // cached value because it must be looked up quickly and a live registration
  // doesn't necessarily exist whenever there is a live version.
  blink::mojom::NavigationPreloadState navigation_preload_state_;
  ServiceWorkerMetrics::Site site_for_uma_;

  Status status_ = NEW;
  std::unique_ptr<EmbeddedWorkerInstance> embedded_worker_;
  std::vector<StatusCallback> start_callbacks_;
  std::vector<base::OnceClosure> stop_callbacks_;
  std::vector<base::OnceClosure> status_change_callbacks_;

  // Holds in-flight requests, including requests due to outstanding push,
  // fetch, sync, etc. events.
  base::IDMap<std::unique_ptr<InflightRequest>> inflight_requests_;

  // Keeps track of in-flight requests for timeout purposes. Requests are sorted
  // by their expiration time (soonest to expire at the beginning of the
  // set). The timeout timer periodically checks |request_timeouts_| for entries
  // that should time out.
  std::set<InflightRequestTimeoutInfo> request_timeouts_;

  // Container for pending external requests for this service worker.
  // (key, value): (request uuid, request id).
  using RequestUUIDToRequestIDMap = std::map<std::string, int>;
  RequestUUIDToRequestIDMap external_request_uuid_to_request_id_;

  // List of UUIDs of external requests that were issued before this worker
  // reached RUNNING.
  std::set<std::string> pending_external_requests_;

  // Connected to ServiceWorkerContextClient while the worker is running.
  mojo::Remote<blink::mojom::ServiceWorker> service_worker_remote_;

  // Connection to the controller service worker.
  // |controller_receiver_| is non-null only when the |remote_controller_| is
  // requested before the worker is started, it is passed to the worker (and
  // becomes null) once it's started.
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote_controller_;
  mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
      controller_receiver_;

  std::unique_ptr<ServiceWorkerInstalledScriptsSender>
      installed_scripts_sender_;

  std::vector<SkipWaitingCallback> pending_skip_waiting_requests_;
  base::TimeTicks skip_waiting_time_;
  base::TimeTicks no_controllees_time_;

  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerHost> receiver_{this};

  // Set to true if the worker has no inflight events and the idle timer has
  // been triggered. Set back to false if another event starts since the worker
  // is no longer idle.
  bool worker_is_idle_on_renderer_ = true;

  // Set to true when the worker needs to be terminated as soon as possible
  // (e.g. activation).
  bool needs_to_be_terminated_asap_ = false;

  // Keeps track of the provider hosting this running service worker for this
  // version. |provider_host_| is always valid as long as this version is
  // running.
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;

  std::map<std::string, ServiceWorkerProviderHost*> controllee_map_;
  // Will be null while shutting down.
  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::ObserverList<Observer>::Unchecked observers_;
  ServiceWorkerScriptCacheMap script_cache_map_;
  base::OneShotTimer update_timer_;

  // For scheduling Soft Update after main resource requests. See
  // IncrementPendingUpdateHintCount() documentation.
  int pending_update_hint_count_ = 0;

  // Starts running in StartWorker and continues until the worker is stopped.
  base::RepeatingTimer timeout_timer_;
  // Holds the time that the outstanding StartWorker() request started.
  base::TimeTicks start_time_;
  // Holds the time the worker entered STOPPING status. This is also used as a
  // trace event id.
  base::TimeTicks stop_time_;
  // Holds the time the worker was detected as stale and needs updating. We try
  // to update once the worker stops, but will also update if it stays alive too
  // long.
  base::TimeTicks stale_time_;
  // The latest expiration time of all requests that have ever been started. In
  // particular this is not just the maximum of the expiration times of all
  // currently existing requests, but also takes into account the former
  // expiration times of finished requests.
  base::TimeTicks max_request_expiration_time_;

  bool skip_waiting_ = false;
  bool skip_recording_startup_time_ = false;
  bool force_bypass_cache_for_scripts_ = false;
  bool is_update_scheduled_ = false;
  bool in_dtor_ = false;

  // For service worker update checks. Non-null if pause after download during
  // startup was requested. Once paused, the callback is run and reset to
  // null.
  base::OnceClosure pause_after_download_callback_;

  std::unique_ptr<net::HttpResponseInfo> main_script_http_info_;

  // DevTools requires each service worker's script receive time, even for
  // the ones that haven't started. However, a ServiceWorkerVersion's field
  // |main_script_http_info_| is not set until starting up. Rather than
  // reading HttpResponseInfo for all service workers from disk cache and
  // populating |main_script_http_info_| just in order to expose that timestamp,
  // we provide that timestamp here.
  base::Time script_response_time_for_devtools_;

  std::unique_ptr<blink::TrialTokenValidator::FeatureToTokensMap>
      origin_trial_tokens_;

  // If not OK, the reason that StartWorker failed. Used for
  // running |start_callbacks_|.
  blink::ServiceWorkerStatusCode start_worker_status_ =
      blink::ServiceWorkerStatusCode::kOk;

  // The clock used to vend tick time.
  const base::TickClock* tick_clock_;

  // The clock used for actual (wall clock) time
  base::Clock* clock_;

  ServiceWorkerPingController ping_controller_;

  bool stop_when_devtools_detached_ = false;

  // This is the set of features that were used up until installation of this
  // version completed, or used during the lifetime of |this|.
  std::set<blink::mojom::WebFeature> used_features_;

  std::unique_ptr<blink::TrialTokenValidator> validator_;

  // Stores the result of byte-to-byte update check for each script. Used only
  // when ServiceWorkerImportedScriptUpdateCheck is enabled.
  std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>
      compared_script_info_map_;

  // ServiceWorkerImportedScriptUpdateCheck:
  // If this version was created for an update check that found an update,
  // |updated_script_url_| is the URL of the script for which a byte-for-byte
  // change was found. Otherwise, it's the empty GURL.
  GURL updated_script_url_;

  // This holds a mojo interface pointer info to this instance until
  // InitializeGlobalScope() is called.
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerHost>
      service_worker_host_;

  // TODO(crbug.com/951571): Remove once the bug is debugged.
  // This is set when this service worker becomes redundant.
  base::debug::StackTrace redundant_state_callstack_;

  base::WeakPtrFactory<ServiceWorkerVersion> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersion);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
