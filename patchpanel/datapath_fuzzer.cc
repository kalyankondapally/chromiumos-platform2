// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <net/if.h>

#include <fuzzer/FuzzedDataProvider.h>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/logging.h>

#include "patchpanel/datapath.h"
#include "patchpanel/minijailed_process_runner.h"
#include "patchpanel/multicast_forwarder.h"
#include "patchpanel/net_util.h"

namespace patchpanel {

// Always succeeds
int ioctl_stub(int fd, unsigned long req, ...) {
  return 0;
}

class RandomProcessRunner : public MinijailedProcessRunner {
 public:
  explicit RandomProcessRunner(FuzzedDataProvider* data_provider)
      : data_provider_{data_provider} {}
  ~RandomProcessRunner() = default;

  int Run(const std::vector<std::string>& argv, bool log_failures) override {
    return data_provider_->ConsumeBool();
  }

 private:
  FuzzedDataProvider* data_provider_;

  DISALLOW_COPY_AND_ASSIGN(RandomProcessRunner);
};

namespace {

class Environment {
 public:
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);  // <- DISABLE LOGGING.
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Turn off logging.
  logging::SetMinLogLevel(logging::LOG_FATAL);

  FuzzedDataProvider provider(data, size);
  RandomProcessRunner runner(&provider);
  Datapath datapath(&runner, ioctl_stub);

  while (provider.remaining_bytes() > 0) {
    std::string ifname = provider.ConsumeRandomLengthString(IFNAMSIZ - 1);
    std::string bridge = provider.ConsumeRandomLengthString(IFNAMSIZ - 1);
    uint32_t addr = provider.ConsumeIntegral<uint32_t>();
    std::string addr_str = IPv4AddressToString(addr);
    uint32_t prefix_len = provider.ConsumeIntegralInRange<uint32_t>(0, 31);
    Subnet subnet(provider.ConsumeIntegral<int32_t>(), prefix_len,
                  base::DoNothing());
    std::unique_ptr<SubnetAddress> subnet_addr = subnet.AllocateAtOffset(0);
    MacAddress mac;
    std::vector<uint8_t> bytes = provider.ConsumeBytes<uint8_t>(mac.size());
    std::copy(std::begin(bytes), std::begin(bytes), std::begin(mac));

    datapath.AddBridge(ifname, addr, prefix_len);
    datapath.RemoveBridge(ifname);
    datapath.AddInboundIPv4DNAT(ifname, addr_str);
    datapath.RemoveInboundIPv4DNAT(ifname, addr_str);
    datapath.AddVirtualInterfacePair(ifname, bridge);
    datapath.ToggleInterface(ifname, provider.ConsumeBool());
    datapath.ConfigureInterface(ifname, mac, addr, prefix_len,
                                provider.ConsumeBool(), provider.ConsumeBool());
    datapath.RemoveInterface(ifname);
    datapath.AddTAP(ifname, &mac, subnet_addr.get(), "");
    datapath.RemoveTAP(ifname);
    datapath.AddIPv4Route(provider.ConsumeIntegral<uint32_t>(),
                          provider.ConsumeIntegral<uint32_t>(),
                          provider.ConsumeIntegral<uint32_t>());
  }

  return 0;
}

}  // namespace
}  // namespace patchpanel