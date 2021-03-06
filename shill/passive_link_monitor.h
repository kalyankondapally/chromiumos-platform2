// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PASSIVE_LINK_MONITOR_H_
#define SHILL_PASSIVE_LINK_MONITOR_H_

#include <memory>

#include <base/callback.h>
#include <base/cancelable_callback.h>

#include "shill/refptr_types.h"

namespace shill {

class ArpClient;
class EventDispatcher;
class IOHandler;
class IOHandlerFactory;

// PassiveLinkMonitor tracks the status of a connection by monitoring ARP
// requests received on the given interface. Each cycle consist of 25 seconds,
// with at lease 5 ARP requests expected in a cycle, a callback indicating
// failure will be invoke if that expectation is not met. Caller can specify
// number of cycles to monitor, once that number is reached without any
// failures, a callback indicating success will be invoked. Monitor will
// automatically stop when the monitor results in either failure or success.
class PassiveLinkMonitor {
 public:
  using ResultCallback = base::Callback<void(bool)>;

  // The default number of cycles to monitor for.
  static const int kDefaultMonitorCycles;

  PassiveLinkMonitor(const ConnectionRefPtr& connection,
                     EventDispatcher* dispatcher,
                     const ResultCallback& result_callback);
  virtual ~PassiveLinkMonitor();

  // Starts passive link-monitoring for the specified number of cycles.
  virtual bool Start(int num_cycles);
  // Stop passive link-monitoring. Clears any accumulated statistics.
  virtual void Stop();

 private:
  friend class PassiveLinkMonitorTest;

  bool StartArpClient();
  void StopArpClient();

  // Callback to be invoked whenever the ARP reception socket has data
  // available to be received.
  void ReceiveRequest(int fd);
  // Callback to be invoked when cycle period is reached without receiving
  // the expected number of ARP requests.
  void CycleTimeoutHandler();
  // Method to be called when the monitor is completed.
  void MonitorCompleted(bool status);

  // The connection on which to perform passive link monitoring.
  ConnectionRefPtr connection_;
  // The dispatcher on which to create delayed tasks.
  EventDispatcher* dispatcher_;
  // ArpClient instance for monitoring ARP requests.
  std::unique_ptr<ArpClient> arp_client_;
  // Callback to be invoked when monitor is completed, either failure or
  // success.
  ResultCallback result_callback_;

  // Number of cycles to monitor for.
  int num_cycles_to_monitor_;
  // Number of ARP requests received in current cycle.
  int num_requests_received_;
  // Number of cycles passed so far.
  int num_cycles_passed_;

  IOHandlerFactory* io_handler_factory_;
  // IOCallback that fires when the socket associated with our ArpClient
  // has a packet to be received.  Calls ReceiveRequest().
  std::unique_ptr<IOHandler> receive_request_handler_;
  // Callback for handling cycle timeout.
  base::CancelableClosure monitor_cycle_timeout_callback_;
  // Callback for handling monitor completed event.
  base::CancelableClosure monitor_completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(PassiveLinkMonitor);
};

}  // namespace shill

#endif  // SHILL_PASSIVE_LINK_MONITOR_H_
