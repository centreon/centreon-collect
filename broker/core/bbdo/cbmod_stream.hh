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
#ifndef CCB_BBDO_CBMOD_STREAM_HH
#define CCB_BBDO_CBMOD_STREAM_HH
#include "broker/core/bbdo/stream.hh"
#include "broker/core/config/applier/cbmod_state.hh"

namespace com::centreon::broker::bbdo {
class cbmod_stream : public stream {
  config::applier::cbmod_state& _state;

 protected:
  void _handle_bbdo_event(const std::shared_ptr<io::data>& d) override;

 public:
  cbmod_stream(bool is_input,
               bool grpc_serialized = false,
               const std::list<std::shared_ptr<io::extension>>& extensions = {})
      : stream(is_input, grpc_serialized, extensions),
        _state{static_cast<config::applier::cbmod_state&>(
            config::applier::state::instance())} {}
  bool supports_centralized_conf() const override;
  void specific_negotiate(Welcome& obj) override;
  void send_engine_conf(
      std::unique_ptr<com::centreon::engine::configuration::State>& conf);
  uint32_t write(const std::shared_ptr<io::data>& d) override;
  uint32_t stop() override;
};
}  // namespace com::centreon::broker::bbdo

#endif /* !CCB_BBDO_CBMOD_STREAM_HH */
