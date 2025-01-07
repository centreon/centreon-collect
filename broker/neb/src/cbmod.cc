/**
 * Copyright 2024 Centreon
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
#include "com/centreon/broker/neb/cbmod.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/common/utf8.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/nebstructs.hh"
#include "com/centreon/engine/severity.hh"
#include "com/centreon/engine/tag.hh"
#include "common/log_v2/log_v2.hh"
#include "compatibility/common.h"

using com::centreon::common::log_v2::log_v2;

namespace com::centreon::broker::neb {
class cbmodimpl {
  std::shared_ptr<spdlog::logger> _neb_logger;
  multiplexing::publisher _publisher;

 public:
  cbmodimpl() {}
  cbmodimpl& operator=(const cbmodimpl&) = delete;

  void process_data(const nebstruct_process_data* ds,
                    const std::string& program_version);
  const multiplexing::publisher& publisher() const { return _publisher; }
  multiplexing::publisher& mut_publisher() { return _publisher; }
};

cbmod::cbmod(const std::string& config_file, const std::string& proto_conf)
    : _neb_logger{log_v2::instance().get(log_v2::NEB)},
      _impl{new cbmodimpl},
      _proto_conf{proto_conf},
      _use_protobuf{
          config::applier::state::instance().get_bbdo_version().major_v > 2} {
  // Try configuration parsing.
  com::centreon::broker::config::parser p;
  com::centreon::broker::config::state s{p.parse(config_file)};

  bool new_generation = !proto_conf.empty();

  s.set_extended_negotiation(new_generation);

  // Initialization.
  /* This is a little hack to avoid to replace the log file set by
   * centengine */
  s.mut_log_conf().allow_only_atomic_changes(true);
  com::centreon::broker::config::applier::init(com::centreon::common::ENGINE,
                                               s);
  try {
    log_v2::instance().apply(s.log_conf());
  } catch (const std::exception& e) {
    log_v2::instance().get(log_v2::CORE)->error("main: {}", e.what());
  }

  com::centreon::broker::config::applier::state::instance().apply(s);
}

cbmod::~cbmod() noexcept = default;

uint64_t cbmod::poller_id() const {
  return config::applier::state::instance().poller_id();
}

const std::string& cbmod::poller_name() const {
  return config::applier::state::instance().poller_name();
}

void cbmod::write(const std::shared_ptr<io::data>& msg) {
  _impl->mut_publisher().write(msg);
}

const bbdo::bbdo_version cbmod::bbdo_version() const {
  return config::applier::state::instance().get_bbdo_version();
}

/**
 * @brief Tells us if neb events are sent using protobuf or not.
 *
 * @return True if we use protobuf to send them (bbdo version >= 3.0.0).
 */
bool cbmod::use_protobuf() const {
  return _use_protobuf;
}

void cbmod::add_acknowledgement(
    const std::shared_ptr<neb::acknowledgement>& ack) {
  _acknowledgements[std::make_pair(ack->host_id, ack->service_id)] = ack;
}

void cbmod::add_acknowledgement(
    const std::shared_ptr<neb::pb_acknowledgement>& ack) {
  Acknowledgement& obj = static_cast<Acknowledgement&>(ack->mut_obj());
  _acknowledgements[std::make_pair(obj.host_id(), obj.service_id())] = ack;
}

}  // namespace com::centreon::broker::neb
