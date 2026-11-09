/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */
#ifndef CCB_BBDO_BROKER_STREAM_HH
#define CCB_BBDO_BROKER_STREAM_HH
#include "broker/core/bbdo/stream.hh"
#include "broker/core/config/applier/broker_state.hh"

namespace com::centreon::broker::bbdo {
class broker_stream : public stream {
  config::applier::broker_state& _state;

  /* broker_state configuration generation seen at the last configuration
   * relay check done in read(): if it did not change since, the check is
   * skipped. The initial value can't be a real generation, so the first
   * read() always checks. No need to track the peer identity: it is set once
   * in negotiate(), that the acceptor/connector call before the stream is
   * read, so the first check is already done with the right identity. */
  uint64_t _last_config_generation = UINT64_MAX;

  void _push_pending_config_messages();

 protected:
  void _send_diff_state_for_poller(uint64_t poller_id);
  void _handle_bbdo_event(const std::shared_ptr<io::data>& d) override;

 public:
  broker_stream(
      bool is_input,
      bool grpc_serialized = false,
      const std::list<std::shared_ptr<io::extension>>& extensions = {})
      : stream(is_input, grpc_serialized, extensions),
        _state{static_cast<config::applier::broker_state&>(
            config::applier::state::instance())} {}
  bool supports_centralized_conf() const override;
  void specific_negotiate(Welcome& obj) override;
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  uint32_t stop() override;
};
}  // namespace com::centreon::broker::bbdo

#endif /* !CCB_BBDO_BROKER_STREAM_HH */
