// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_SHARED_RESOURCE_SCHEDULER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_SHARED_RESOURCE_SCHEDULER_H_

#include <list>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace chromeos {

namespace secure_channel {

// Schedules requests for shared resources. When requested connections require
// using a system resouce which is limited (e.g., a limited number of Bluetooth
// advertisements can be present at one time), requests are queued up.
//
// SharedResourceScheduler returns the highest-priority request first. If two
// requests have been provided that have the same priority, the one which was
// provided to this class is returned first.
class SharedResourceScheduler {
 public:
  SharedResourceScheduler();
  virtual ~SharedResourceScheduler();

  // Schedules a request to use a shared resource.
  void ScheduleRequest(const DeviceIdPair& request,
                       ConnectionPriority connection_priority);

  // Updates a previously-scheduled request to a new priority.
  void UpdateRequestPriority(const DeviceIdPair& request,
                             ConnectionPriority connection_priority);

  // Removes a request from the scheduler.
  void RemoveScheduledRequest(const DeviceIdPair& request);

  // Returns the next scheduled request, or base::nullopt if there are no
  // requests scheduled. Once a request is retrieved via this function, it is
  // removed from the scheduler and will not be re-scheduled unless a new call
  // to ScheduleRequest() is made.
  base::Optional<std::pair<DeviceIdPair, ConnectionPriority>>
  GetNextScheduledRequest();

  // Returns the priority of the the request which will next be returned by
  // GetNextScheduledRequest(). If no requests are currently scheduled,
  // base::nullopt is returned.
  base::Optional<ConnectionPriority> GetHighestPriorityOfScheduledRequests();

  bool empty() const { return request_to_priority_map_.empty(); }

 private:
  friend class FakeBleAdvertiser;

  // Map from priority to a list of pending requests. Each list is ordered such
  // that requests that should be processed first reside before requests that
  // should be processed afterward.
  base::flat_map<ConnectionPriority, std::list<DeviceIdPair>>
      priority_to_queued_requests_map_;

  // Map from request to its priority.
  base::flat_map<DeviceIdPair, ConnectionPriority> request_to_priority_map_;

  DISALLOW_COPY_AND_ASSIGN(SharedResourceScheduler);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_SHARED_RESOURCE_SCHEDULER_H_
