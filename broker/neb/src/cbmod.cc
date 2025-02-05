/**
 * Copyright 2024-2025 Centreon
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
#include "com/centreon/broker/neb/internal.hh"
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

  const multiplexing::publisher& publisher() const { return _publisher; }
  multiplexing::publisher& mut_publisher() { return _publisher; }
};

cbmod::cbmod(const std::string& config_file)
    : _neb_logger{log_v2::instance().get(log_v2::NEB)}, _impl{new cbmodimpl} {
  // Try configuration parsing.
  com::centreon::broker::config::parser p;
  com::centreon::broker::config::state s{p.parse(config_file)};

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

  /* Once the configuration is applied, we can know if we use protobuf or not */
  _use_protobuf =
      config::applier::state::instance().get_bbdo_version().major_v > 2;
}

/**
 * @brief Constructor of the cbmod class. Useful in unit tests.
 */
cbmod::cbmod()
    : _neb_logger{log_v2::instance().get(log_v2::NEB)},
      _impl{new cbmodimpl},
      _proto_conf{""} {
  com::centreon::broker::config::state s;
  s.poller_id(1);
  s.poller_name("test");
  com::centreon::broker::config::applier::init(com::centreon::common::ENGINE,
                                               s);
  _use_protobuf =
      config::applier::state::instance().get_bbdo_version().major_v > 2;

  com::centreon::broker::config::applier::state::instance().apply(s);
}

cbmod::~cbmod() noexcept {
  _neb_logger->debug("cbmod: destruction...");
  while (!_downtimes.empty()) {
    uint64_t downtime_id = _downtimes.begin()->first;
    remove_downtime(downtime_id);
  }
  config::applier::deinit();
  _neb_logger->debug("cbmod: destruction... Done");
}

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

/**
 * @brief Add an acknowledgement to the cbmod list.
 *
 * @param ack The acknowledgement.
 */
void cbmod::add_acknowledgement(
    const std::shared_ptr<neb::acknowledgement>& ack) {
  auto new_ack = std::make_shared<neb::pb_acknowledgement>();
  auto& obj = new_ack->mut_obj();
  obj.set_host_id(ack->host_id);
  obj.set_service_id(ack->service_id);
  obj.set_instance_id(ack->poller_id);
  obj.set_type(
      static_cast<Acknowledgement_ResourceType>(ack->acknowledgement_type));
  obj.set_author(ack->author);
  obj.set_comment_data(ack->comment);
  obj.set_sticky(ack->is_sticky);
  obj.set_notify_contacts(ack->notify_contacts);
  obj.set_entry_time(ack->entry_time);
  obj.set_deletion_time(ack->deletion_time);
  obj.set_persistent_comment(ack->persistent_comment);
  obj.set_state(ack->state);

  _acknowledgements[std::make_pair(ack->host_id, ack->service_id)] = new_ack;
}

/**
 * @brief Add an acknowledgement to the cbmod list.
 *
 * @param ack The acknowledgement.
 */
void cbmod::add_acknowledgement(
    const std::shared_ptr<neb::pb_acknowledgement>& ack) {
  const Acknowledgement& obj = static_cast<const Acknowledgement&>(ack->obj());
  _acknowledgements[std::make_pair(obj.host_id(), obj.service_id())] = ack;
}

/**
 * @brief Find an acknowledgement from its host ID and service ID in the cbmod
 * list.
 *
 * @param host_id The host ID.
 * @param service_id The service ID.
 *
 * @return The acknowledgement if found, nullptr otherwise.
 */
std::shared_ptr<neb::pb_acknowledgement> cbmod::find_acknowledgement(
    uint64_t host_id,
    uint64_t service_id) const {
  auto it = _acknowledgements.find(std::make_pair(host_id, service_id));
  if (it != _acknowledgements.end()) {
    return it->second;
  }
  return nullptr;
}

/**
 * @brief Remove an acknowledgement on resource given by its host ID and service
 * ID from the cbmod list.
 *
 * @param host_id The host ID.
 * @param service_id The service ID.
 */
void cbmod::remove_acknowledgement(uint64_t host_id, uint64_t service_id) {
  _acknowledgements.erase(std::make_pair(host_id, service_id));
}

/**
 * @brief Accessor to the number of acknowledgements in the cbmod list.
 *
 * @return The number of acknowledgements.
 */
size_t cbmod::acknowledgements_count() const {
  return _acknowledgements.size();
}

/**
 * @brief Add a downtime to the cbmod list.
 *
 * @param downtime_id The downtime ID.
 * @param host_id The host ID.
 * @param service_id The service ID.
 * @param author_name The author name.
 * @param comment_data The comment data.
 * @param downtime_type The downtime type.
 * @param entry_time The entry time.
 * @param end_time The end time.
 * @param duration The duration.
 * @param triggered_by The downtime ID of the downtime that triggered this one.
 * @param fixed True if the downtime is fixed.
 */
void cbmod::add_downtime(uint64_t downtime_id,
                         uint64_t host_id,
                         uint64_t service_id,
                         const char* author_name,
                         const char* comment_data,
                         int downtime_type,
                         time_t entry_time,
                         time_t start_time,
                         time_t end_time,
                         uint32_t duration,
                         uint64_t triggered_by,
                         bool fixed) {
  auto pb_dt = std::make_shared<pb_downtime>();
  auto& obj = pb_dt->mut_obj();
  obj.set_id(downtime_id);
  obj.set_instance_id(poller_id());
  obj.set_host_id(host_id);
  obj.set_service_id(service_id);
  if (author_name)
    obj.set_author(common::check_string_utf8(author_name));
  if (comment_data)
    obj.set_comment_data(common::check_string_utf8(comment_data));
  obj.set_type(
      static_cast<com::centreon::broker::Downtime_DowntimeType>(downtime_type));
  obj.set_duration(duration);
  obj.set_triggered_by(triggered_by);
  obj.set_entry_time(entry_time);
  obj.set_actual_start_time(-1);
  obj.set_start_time(start_time);
  obj.set_end_time(end_time);
  obj.set_deletion_time(-1);
  obj.set_cancelled(false);
  obj.set_actual_end_time(-1);
  obj.set_fixed(fixed);
  _downtimes[downtime_id] = pb_dt;
}

/**
 * @brief Translate a protobuf downtime to a legacy downtime.
 *
 * @param pb_dt The protobuf downtime.
 *
 * @return The legacy downtime.
 */
static std::shared_ptr<neb::downtime> translate_to_legacy_downtime(
    const std::shared_ptr<pb_downtime>& pb_dt) {
  auto retval = std::make_shared<neb::downtime>();
  const Downtime& obj = static_cast<const Downtime&>(pb_dt->obj());
  retval->actual_end_time = obj.actual_end_time();
  retval->actual_start_time = obj.actual_start_time();
  retval->author = obj.author();
  retval->downtime_type = obj.type();
  retval->deletion_time = obj.deletion_time();
  retval->duration = obj.duration();
  retval->end_time = obj.end_time();
  retval->entry_time = obj.entry_time();
  retval->fixed = obj.fixed();
  retval->host_id = obj.host_id();
  retval->poller_id = obj.instance_id();
  retval->internal_id = obj.id();
  retval->service_id = obj.service_id();
  retval->start_time = obj.start_time();
  retval->triggered_by = obj.triggered_by();
  retval->was_cancelled = obj.cancelled();
  retval->was_started = obj.started();
  retval->comment = obj.comment_data();
  return retval;
}

/**
 * @brief Start a downtime.
 *
 * @param downtime_id The downtime ID.
 */
void cbmod::start_downtime(uint64_t downtime_id) {
  auto pb_dt = _downtimes[downtime_id];
  assert(pb_dt);
  auto& obj = pb_dt->mut_obj();
  obj.set_started(true);
  obj.set_actual_start_time(time(nullptr));
  if (_use_protobuf)
    write(pb_dt);
  else
    write(translate_to_legacy_downtime(pb_dt));
}

/**
 * @brief Stop a downtime.
 *
 * @param downtime_id The downtime ID.
 * @param cancelled True if the downtime was cancelled.
 */
void cbmod::stop_downtime(uint64_t downtime_id, bool cancelled) {
  auto pb_dt = _downtimes[downtime_id];
  assert(pb_dt);
  auto& obj = pb_dt->mut_obj();
  obj.set_cancelled(cancelled);
  obj.set_actual_end_time(time(nullptr));
  if (_use_protobuf)
    write(pb_dt);
  else
    write(translate_to_legacy_downtime(pb_dt));
}

/**
 * @brief Remove a downtime from the cbmod list.
 *
 * @param downtime_id The downtime ID.
 */
void cbmod::remove_downtime(uint64_t downtime_id) {
  auto found = _downtimes.find(downtime_id);
  if (found != _downtimes.end()) {
    auto pb_dt = found->second;
    auto& obj = pb_dt->mut_obj();
    if (!obj.started())
      obj.set_cancelled(true);
    time_t now = time(nullptr);
    obj.set_deletion_time(now);
    obj.set_deletion_time(now);
    if (_use_protobuf)
      write(pb_dt);
    else
      write(translate_to_legacy_downtime(pb_dt));
    _downtimes.erase(found);
  }
}

/**
 * @brief When centengine is reloaded, update the cbmod.
 */
void cbmod::reload() {
  if (com::centreon::broker::config::applier::state::instance()
          .get_bbdo_version()
          .major_v > 2) {
    auto ic = std::make_shared<neb::pb_instance_configuration>();
    auto& obj = ic->mut_obj();
    obj.set_loaded(true);
    obj.set_poller_id(config::applier::state::instance().poller_id());
    write(ic);
  } else {
    auto ic = std::make_shared<neb::instance_configuration>();
    ic->loaded = true;
    ic->poller_id = config::applier::state::instance().poller_id();
    write(ic);
  }
}
}  // namespace com::centreon::broker::neb
