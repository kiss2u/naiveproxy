// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_QBONE_BONNET_TUN_DEVICE_CONTROLLER_H_
#define QUICHE_QUIC_QBONE_BONNET_TUN_DEVICE_CONTROLLER_H_

#include "quic/qbone/bonnet/tun_device.h"
#include "quic/qbone/platform/netlink_interface.h"
#include "quic/qbone/qbone_control.pb.h"
#include "quic/qbone/qbone_control_stream.h"

namespace quic {

// TunDeviceController consumes control stream messages from a Qbone server
// and applies the given updates to the TUN device.
class TunDeviceController {
 public:
  // |ifname| is the interface name of the TUN device to be managed. This does
  // not take ownership of |netlink|.
  TunDeviceController(std::string ifname, bool setup_tun,
                      NetlinkInterface* netlink)
      : ifname_(std::move(ifname)), setup_tun_(setup_tun), netlink_(netlink) {}

  TunDeviceController(const TunDeviceController&) = delete;
  TunDeviceController& operator=(const TunDeviceController&) = delete;

  TunDeviceController(TunDeviceController&&) = delete;
  TunDeviceController& operator=(TunDeviceController&&) = delete;

  virtual ~TunDeviceController() = default;

  // Updates the local address of the TUN device to be the first address in the
  // given |response.ip_range()|.
  virtual bool UpdateAddress(const IpRange& desired_range);

  // Updates the set of routes that the TUN device will provide. All current
  // routes for the tunnel that do not exist in the |response| will be removed.
  virtual bool UpdateRoutes(const IpRange& desired_range,
                            const std::vector<IpRange>& desired_routes);

  virtual QuicIpAddress current_address();

 private:
  // Update the IP Rules, this should only be used by UpdateRoutes.
  bool UpdateRules(IpRange desired_range);

  const std::string ifname_;
  const bool setup_tun_;

  NetlinkInterface* netlink_;

  QuicIpAddress current_address_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_QBONE_BONNET_TUN_DEVICE_CONTROLLER_H_
