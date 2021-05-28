// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quic/core/quic_idle_network_detector.h"

#include "quic/core/quic_constants.h"

namespace quic {

namespace {

class AlarmDelegate : public QuicAlarm::Delegate {
 public:
  explicit AlarmDelegate(QuicIdleNetworkDetector* detector)
      : detector_(detector) {}
  AlarmDelegate(const AlarmDelegate&) = delete;
  AlarmDelegate& operator=(const AlarmDelegate&) = delete;

  void OnAlarm() override { detector_->OnAlarm(); }

 private:
  QuicIdleNetworkDetector* detector_;
};

}  // namespace

QuicIdleNetworkDetector::QuicIdleNetworkDetector(
    Delegate* delegate,
    QuicTime now,
    QuicConnectionArena* arena,
    QuicAlarmFactory* alarm_factory)
    : delegate_(delegate),
      start_time_(now),
      handshake_timeout_(QuicTime::Delta::Infinite()),
      time_of_last_received_packet_(now),
      time_of_first_packet_sent_after_receiving_(QuicTime::Zero()),
      idle_network_timeout_(QuicTime::Delta::Infinite()),
      alarm_(
          alarm_factory->CreateAlarm(arena->New<AlarmDelegate>(this), arena)) {}

void QuicIdleNetworkDetector::OnAlarm() {
  if (handshake_timeout_.IsInfinite()) {
    delegate_->OnIdleNetworkDetected();
    return;
  }
  if (idle_network_timeout_.IsInfinite()) {
    delegate_->OnHandshakeTimeout();
    return;
  }
  if (last_network_activity_time() + idle_network_timeout_ >
      start_time_ + handshake_timeout_) {
    delegate_->OnHandshakeTimeout();
    return;
  }
  delegate_->OnIdleNetworkDetected();
}

void QuicIdleNetworkDetector::SetTimeouts(
    QuicTime::Delta handshake_timeout,
    QuicTime::Delta idle_network_timeout) {
  handshake_timeout_ = handshake_timeout;
  idle_network_timeout_ = idle_network_timeout;

  SetAlarm();
}

void QuicIdleNetworkDetector::StopDetection() {
  alarm_->Cancel();
  handshake_timeout_ = QuicTime::Delta::Infinite();
  idle_network_timeout_ = QuicTime::Delta::Infinite();
}

void QuicIdleNetworkDetector::OnPacketSent(QuicTime now,
                                           QuicTime::Delta pto_delay) {
  if (time_of_first_packet_sent_after_receiving_ >
      time_of_last_received_packet_) {
    return;
  }
  time_of_first_packet_sent_after_receiving_ =
      std::max(time_of_first_packet_sent_after_receiving_, now);
  if (shorter_idle_timeout_on_sent_packet_) {
    MaybeSetAlarmOnSentPacket(pto_delay);
    return;
  }

  SetAlarm();
}

void QuicIdleNetworkDetector::OnPacketReceived(QuicTime now) {
  time_of_last_received_packet_ = std::max(time_of_last_received_packet_, now);

  SetAlarm();
}

void QuicIdleNetworkDetector::SetAlarm() {
  // Set alarm to the nearer deadline.
  QuicTime new_deadline = QuicTime::Zero();
  if (!handshake_timeout_.IsInfinite()) {
    new_deadline = start_time_ + handshake_timeout_;
  }
  if (!idle_network_timeout_.IsInfinite()) {
    const QuicTime idle_network_deadline = GetIdleNetworkDeadline();
    if (new_deadline.IsInitialized()) {
      new_deadline = std::min(new_deadline, idle_network_deadline);
    } else {
      new_deadline = idle_network_deadline;
    }
  }
  alarm_->Update(new_deadline, kAlarmGranularity);
}

void QuicIdleNetworkDetector::MaybeSetAlarmOnSentPacket(
    QuicTime::Delta pto_delay) {
  QUICHE_DCHECK(shorter_idle_timeout_on_sent_packet_);
  if (!handshake_timeout_.IsInfinite() || !alarm_->IsSet()) {
    SetAlarm();
    return;
  }
  // Make sure connection will be alive for another PTO.
  const QuicTime deadline = alarm_->deadline();
  const QuicTime min_deadline = last_network_activity_time() + pto_delay;
  if (deadline > min_deadline) {
    return;
  }
  alarm_->Update(min_deadline, kAlarmGranularity);
}

QuicTime QuicIdleNetworkDetector::GetIdleNetworkDeadline() const {
  if (idle_network_timeout_.IsInfinite()) {
    return QuicTime::Zero();
  }
  return last_network_activity_time() + idle_network_timeout_;
}

}  // namespace quic
