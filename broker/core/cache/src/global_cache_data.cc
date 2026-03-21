/**
 * Copyright 2023 Centreon
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

#include "com/centreon/broker/cache/protobuf.hh"

#include <boost/thread/lock_types.hpp>
#include "bbdo/bam/dimension_ba_bv_relation_event.hh"
#include "bbdo/bam/dimension_ba_event.hh"
#include "bbdo/bam/dimension_bv_event.hh"
#include "bbdo/bam/dimension_truncate_table_signal.hh"
#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "boost/interprocess/detail/segment_manager_helper.hpp"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/bbdo2_to_bbdo3.hh"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/cache/global_cache_data.hh"
#include "com/centreon/broker/cache/protobuf.hh"
#include "com/centreon/broker/cache/protobuf_utils.hh"
#include "com/centreon/broker/neb/custom_variable.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/host_group.hh"
#include "com/centreon/broker/neb/host_group_member.hh"
#include "com/centreon/broker/neb/instance.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/broker/neb/service_group.hh"
#include "com/centreon/broker/neb/service_group_member.hh"
#include "common/log_v2/log_v2.hh"
#include "neb.pb.h"

#define UPDATE_FIELD(field)                   \
  if (to_update.field() != in.field()) {      \
    to_update.mutable_##field() = in.field(); \
    at_least_one_modif = true;                \
  }

#define UPDATE_STRING_FIELD(field)                           \
  if (in.field().compare(to_update.field().c_str())) {       \
    to_update.mutable_##field().assign(in.field().c_str(),   \
                                       in.field().length()); \
    at_least_one_modif = true;                               \
  }

#define UPDATE_OPTIONAL_FIELD(field)                         \
  if (in.has_##field() && to_update.field() != in.field()) { \
    to_update.mutable_##field() = in.field();                \
    at_least_one_modif = true;                               \
  }

#define UPDATE_OPTIONAL_STRING_FIELD(field)                                \
  if (in.has_##field() && in.field().compare(to_update.field().c_str())) { \
    to_update.mutable_##field().assign(in.field().c_str(),                 \
                                       in.field().length());               \
    at_least_one_modif = true;                                             \
  }

using namespace com::centreon::broker::cache;
using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;

global_cache_data::global_cache_data(
    const std::shared_ptr<asio::io_context> io_context,
    const std::string& file_path,
    e_cache_type cache_type,
    const std::shared_ptr<global_cache>& conf_cache,
    unsigned grow_step,
    unsigned nb_update_before_save,
    std::chrono::system_clock::duration save_interval)
    : global_cache(io_context,
                   file_path,
                   log_v2::instance().get(log_v2::CORE),
                   cache_type,
                   conf_cache,
                   grow_step,
                   nb_update_before_save,
                   save_interval) {}

/**
 * @brief Initialize or recover all named containers in the memory-mapped
 * segment.
 *
 * Extends global_cache::managed_map() by finding or constructing every
 * data container (hosts, services, groups, tags, BAM objects, …) that
 * global_cache_data needs.
 *
 * - create=true  : constructs all named containers from scratch. Called when
 *                  the file did not exist or was invalid and has just been
 *                  created.
 * - create=false : looks up each named container in the existing file and
 *                  throws std::invalid_argument if any is missing, which causes
 *                  _open() to delete and recreate the file.
 *
 * In both cases the _allocators helper is (re)created from the current segment
 * manager, which is necessary after any remap (e.g. after a file grow).
 *
 * @param create true if the file was just created, false if reopened.
 * @throws std::invalid_argument if any expected named object is absent.
 */
void global_cache_data::managed_map(bool create) {
  global_cache::managed_map(create);
  _allocators = std::make_unique<allocators>(_file->get_segment_manager());
  if (create) {
    _index_id_mapping = _file->find_or_construct<index_id_mapping>(
        "index_id_mapping")(_file->get_segment_manager());
    _id_to_host = _file->find_or_construct<id_to_host>("id_to_host")(
        _file->get_segment_manager());
    _id_to_service = _file->find_or_construct<id_to_serv>("id_to_service")(
        _file->get_segment_manager());
    _id_to_instance = _file->find_or_construct<id_to_instance>(
        "id_to_instance")(_file->get_segment_manager());
    _id_to_host_group = _file->find_or_construct<id_to_host_group>(
        "host_group")(_file->get_segment_manager());
    _id_to_serv_group = _file->find_or_construct<id_to_serv_group>(
        "service_group")(_file->get_segment_manager());
    _host_group_members = _file->find_or_construct<host_group_cont>(
        "host_group_member")(_file->get_segment_manager());
    _service_group_members = _file->find_or_construct<service_group_cont>(
        "service_group_member")(_file->get_segment_manager());
    _metric_id_mapping = _file->find_or_construct<metric_id_mapping>(
        "metric_id_mapping")(_file->get_segment_manager());
    _id_to_dimension_ba_event =
        _file->find_or_construct<id_to_dimension_ba_event>(
            "id_to_dimension_ba_event")(_file->get_segment_manager());
    _id_to_dimension_bv_event =
        _file->find_or_construct<id_to_dimension_bv_event>(
            "id_to_dimension_bv_event")(_file->get_segment_manager());
    _id_to_dimension_ba_bv_relation =
        _file->find_or_construct<id_to_dimension_ba_bv_relation>(
            "id_to_dimension_ba_bv_relation")(_file->get_segment_manager());
    _id_to_tag = _file->find_or_construct<id_to_tag>("id_to_tag")(
        _file->get_segment_manager());
  } else {
    _index_id_mapping = _file->find<index_id_mapping>("index_id_mapping").first;
    if (!_index_id_mapping) {
      throw std::invalid_argument("index_id_mapping not found");
    }
    _id_to_host = _file->find<id_to_host>("id_to_host").first;
    if (!_id_to_host) {
      throw std::invalid_argument("id_to_host not found");
    }
    _id_to_service = _file->find<id_to_serv>("id_to_service").first;
    if (!_id_to_service) {
      throw std::invalid_argument("id_to_service not found");
    }
    _id_to_instance = _file->find<id_to_instance>("id_to_instance").first;
    if (!_id_to_instance) {
      throw std::invalid_argument("id_to_instance not found");
    }
    _id_to_host_group = _file->find<id_to_host_group>("host_group").first;
    if (!_id_to_host_group) {
      throw std::invalid_argument("host_group not found");
    }
    _id_to_serv_group = _file->find<id_to_serv_group>("service_group").first;
    if (!_id_to_serv_group) {
      throw std::invalid_argument("service_group not found");
    }
    _host_group_members =
        _file->find<host_group_cont>("host_group_member").first;
    if (!_host_group_members) {
      throw std::invalid_argument("host_group_member not found");
    }
    _service_group_members =
        _file->find<service_group_cont>("service_group_member").first;
    if (!_service_group_members) {
      throw std::invalid_argument("service_group_member not found");
    }
    _metric_id_mapping =
        _file->find<metric_id_mapping>("metric_id_mapping").first;
    if (!_metric_id_mapping) {
      throw std::invalid_argument("metric_id_mapping not found");
    }
    _id_to_dimension_ba_event =
        _file->find<id_to_dimension_ba_event>("id_to_dimension_ba_event").first;
    if (!_id_to_dimension_ba_event) {
      throw std::invalid_argument("id_to_dimension_ba_event not found");
    }
    _id_to_dimension_bv_event =
        _file->find<id_to_dimension_bv_event>("id_to_dimension_bv_event").first;
    if (!_id_to_dimension_bv_event) {
      throw std::invalid_argument("id_to_dimension_bv_event not found");
    }
    _id_to_dimension_ba_bv_relation =
        _file
            ->find<id_to_dimension_ba_bv_relation>(
                "id_to_dimension_ba_bv_relation")
            .first;
    if (!_id_to_dimension_ba_bv_relation) {
      throw std::invalid_argument("id_to_dimension_ba_bv_relation not found");
    }
    _id_to_tag = _file->find<id_to_tag>("id_to_tag").first;
    if (!_id_to_tag) {
      throw std::invalid_argument("id_to_tag not found");
    }
  }
}

/**
 * @brief Dispatch an event to the appropriate cache writer(s).
 *
 * Routes the event to _write_rt() or _write_conf() depending on the cache
 * type, then forwards it to the conf cache as well when in real-time mode.
 *
 * If the mapped file runs out of space (interprocess::bad_alloc), the file is
 * grown via allocation_exception_handler() and the write is retried once. On a
 * second failure the event is dropped and a CRITICAL message is logged.
 *
 * @param data           Event to store. No-op if null.
 * @param first_attempt Internal retry guard — callers must use the default
 *                       value (true).
 */
void global_cache_data::_write_impl(const std::shared_ptr<io::data>& data,
                                    bool first_attempt) {
  if (!data)
    return;
  try {
    if (_cache_type == e_cache_type::real_time) {
      _write_rt(data);
      if (_conf_cache) {
        _conf_cache->write(data);
      }
    } else {
      _write_conf(data);
    }
  } catch (const interprocess::bad_alloc& e) {
    if (!first_attempt) {
      SPDLOG_LOGGER_CRITICAL(
          _logger, "cache: fail to write datas to {}, we don't retry again",
          _file_path);
      if (_conf_cache) {
        _conf_cache->write(data);
      }
      return;
    }
    SPDLOG_LOGGER_DEBUG(_logger, "cache: file {} full => grow", _file_path);
    allocation_exception_handler();
    _write_impl(data, false);
  }
}

void global_cache_data::_write_rt(const std::shared_ptr<io::data>& data) {
  switch (data->type()) {
    case neb::host::static_type():
      _process_pb_host(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_host::static_type():
      _process_pb_host(data);
      break;
    case neb::host_status::static_type():
      _process_pb_host_status(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_host_status::static_type():
      _process_pb_host_status(data);
      break;
    case neb::pb_adaptive_host::static_type():
      _process_pb_adaptive_host(data);
      break;
    case neb::pb_adaptive_host_status::static_type():
      _process_pb_adaptive_host_status(data);
      break;
    case neb::service::static_type():
      _process_pb_service(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_service::static_type():
      _process_pb_service(data);
      break;
    case neb::service_status::static_type():
      _process_pb_service_status(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_service_status::static_type():
      _process_pb_service_status(data);
      break;
    case neb::pb_adaptive_service_status::static_type():
      _process_pb_adaptive_service_status(data);
      break;
    case neb::pb_adaptive_service::static_type():
      _process_pb_adaptive_service(data);
      break;
    case bam::dimension_ba_event::static_type():
      _process_dimension_ba_event(bbdo2_to_bbdo3(data));
      break;
    case bam::pb_dimension_ba_event::static_type():
      _process_dimension_ba_event(data);
      break;
    case bam::dimension_bv_event::static_type():
      _process_dimension_bv_event(bbdo2_to_bbdo3(data));
      break;
    case bam::pb_dimension_bv_event::static_type():
      _process_dimension_bv_event(data);
      break;
    case bam::dimension_truncate_table_signal::static_type():
      _process_pb_dimension_truncate_table_signal(bbdo2_to_bbdo3(data));
      break;
    case bam::pb_dimension_truncate_table_signal::static_type():
      _process_pb_dimension_truncate_table_signal(data);
      break;
    default:
      break;
  }
}

void global_cache_data::_write_conf(const std::shared_ptr<io::data>& data) {
  switch (data->type()) {
    case neb::instance::static_type():
      _process_pb_instance(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_instance::static_type():
      _process_pb_instance(data);
      break;
    case neb::host::static_type():
      _process_pb_host(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_host::static_type():
      _process_pb_host(data);
      break;
    case neb::pb_adaptive_host::static_type():
      _process_pb_adaptive_host(data);
      break;
    case neb::host_group::static_type():
      _process_pb_host_group(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_host_group::static_type():
      _process_pb_host_group(data);
      break;
    case neb::host_group_member::static_type():
      _process_pb_host_group_member(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_host_group_member::static_type():
      _process_pb_host_group_member(data);
      break;
    case neb::service::static_type():
      _process_pb_service(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_service::static_type():
      _process_pb_service(data);
      break;
    case neb::pb_adaptive_service::static_type():
      _process_pb_adaptive_service(data);
      break;
    case neb::service_group::static_type():
      _process_pb_service_group(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_service_group::static_type():
      _process_pb_service_group(data);
      break;
    case neb::service_group_member::static_type():
      _process_pb_service_group_member(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_service_group_member::static_type():
      _process_pb_service_group_member(data);
      break;
    case neb::custom_variable::static_type():
      _process_pb_custom_variable(bbdo2_to_bbdo3(data));
      break;
    case neb::pb_custom_variable::static_type():
      _process_pb_custom_variable(data);
      break;
    case storage::pb_index_mapping::static_type():
      _process_index_mapping(data);
      break;
    case storage::index_mapping::static_type():
      _process_index_mapping(bbdo2_to_bbdo3(data));
      break;
    case storage::pb_metric_mapping::static_type():
      _process_metric_mapping(data);
      break;
    case storage::metric_mapping::static_type():
      _process_metric_mapping(bbdo2_to_bbdo3(data));
      break;
    case bam::dimension_ba_event::static_type():
      _process_dimension_ba_event(bbdo2_to_bbdo3(data));
      break;
    case bam::pb_dimension_ba_event::static_type():
      _process_dimension_ba_event(data);
      break;
    case bam::dimension_ba_bv_relation_event::static_type():
      _process_dimension_ba_bv_relation_event(bbdo2_to_bbdo3(data));
      break;
    case bam::pb_dimension_ba_bv_relation_event::static_type():
      _process_dimension_ba_bv_relation_event(data);
      break;
    case bam::dimension_bv_event::static_type():
      _process_dimension_bv_event(bbdo2_to_bbdo3(data));
      break;
    case bam::pb_dimension_bv_event::static_type():
      _process_dimension_bv_event(data);
      break;
    case bam::dimension_truncate_table_signal::static_type():
      _process_pb_dimension_truncate_table_signal(bbdo2_to_bbdo3(data));
      break;
    case bam::pb_dimension_truncate_table_signal::static_type():
      _process_pb_dimension_truncate_table_signal(data);
      break;
    case neb::pb_tag::static_type():
      _process_pb_tag(data);
      break;
    default:
      break;
  }
}

/**
 *  Process an instance event.
 *
 *  @param in  The event.
 */
void global_cache_data::_process_pb_instance(
    std::shared_ptr<io::data> const& data) {
  const auto& in = std::static_pointer_cast<neb::pb_instance>(data);
  boost::unique_lock l(_protect);
  auto exist = _id_to_instance->find(in->obj().instance_id());
  if (exist == _id_to_instance->end()) {
    if (in->obj().running()) {
      instance* to_insert = _file->get_segment_manager()->construct<instance>(
          interprocess::anonymous_instance)(in->obj(), *_allocators);
      _id_to_instance->emplace(in->obj().instance_id(), to_insert);
      _set_dirty_and_increment_modif();
    }
  } else {
    if (in->obj().running()) {
      if (exist->second->update(in->obj(), *_allocators)) {
        _set_dirty_and_increment_modif();
      }
    } else {
      _file->get_segment_manager()->destroy_ptr(exist->second.get());
      _id_to_instance->erase(exist);
      _set_dirty_and_increment_modif();
    }
  }
}

/**
 *  Process a pb host event.
 *
 *  @param h  The event.
 */
void global_cache_data::_process_pb_host(
    std::shared_ptr<io::data> const& data) {
  const auto& in = std::static_pointer_cast<neb::pb_host>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger, "cache: processing host {}", in.host_id());
  boost::unique_lock l(_protect);
  auto exist = _id_to_host->find(in.host_id());
  if (exist == _id_to_host->end()) {
    if (in.enabled()) {
      host* to_insert = _file->get_segment_manager()->construct<host>(
          interprocess::anonymous_instance)(in, *_allocators);
      _id_to_host->emplace(in.host_id(), host_custom_var_pair(to_insert, 0));
      _set_dirty_and_increment_modif();
    }
  } else {
    if (in.enabled()) {
      if (exist->second.first) {
        if (exist->second.first->update(in, *_allocators)) {
          _set_dirty_and_increment_modif();
        }
      } else {
        exist->second.first = _file->get_segment_manager()->construct<host>(
            interprocess::anonymous_instance)(in, *_allocators);
        _set_dirty_and_increment_modif();
      }
    } else {
      if (exist->second.first) {
        _file->get_segment_manager()->destroy_ptr(exist->second.first.get());
      }
      _id_to_host->erase(exist);
      _set_dirty_and_increment_modif();
    }
  }
}

/**
 *  Process a pb host status event.
 *
 *  @param h  The event.
 */
void global_cache_data::_process_pb_host_status(
    std::shared_ptr<io::data> const& data) {
  const auto& in = std::static_pointer_cast<neb::pb_host_status>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger, "cache: processing host status {}",
                      in.host_id());
  boost::unique_lock l(_protect);
  auto exist = _id_to_host->find(in.host_id());
  if (exist == _id_to_host->end() || !exist->second.first) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "Attempt to update host ({}) in cache, but it does not "
        "exist. Maybe Engine should be restarted to update the cache.",
        in.host_id());
    return;
  }
  auto& to_update = *exist->second.first;
  bool at_least_one_modif = false;
  UPDATE_FIELD(checked);
  UPDATE_FIELD(check_type);
  UPDATE_FIELD(state);
  UPDATE_FIELD(state_type);
  UPDATE_FIELD(last_state_change);
  UPDATE_FIELD(last_hard_state);
  UPDATE_FIELD(last_hard_state_change);
  UPDATE_FIELD(last_time_up);
  UPDATE_FIELD(last_time_down);
  UPDATE_FIELD(last_time_unreachable);
  UPDATE_STRING_FIELD(output);
  UPDATE_STRING_FIELD(perfdata);
  UPDATE_FIELD(flapping);
  UPDATE_FIELD(percent_state_change);
  UPDATE_FIELD(latency);
  UPDATE_FIELD(execution_time);
  UPDATE_FIELD(last_check);
  UPDATE_FIELD(next_check);
  UPDATE_FIELD(should_be_scheduled);
  UPDATE_FIELD(check_attempt);
  UPDATE_FIELD(notification_number);
  UPDATE_FIELD(no_more_notifications);
  UPDATE_FIELD(last_notification);
  UPDATE_FIELD(next_host_notification);
  UPDATE_FIELD(acknowledgement_type);
  UPDATE_FIELD(scheduled_downtime_depth);
  if (at_least_one_modif) {
    _set_dirty_and_increment_modif();
  }
}

/**
 * @brief Process a pb adaptive host event.
 *
 * @param data An AdaptiveHostStatus event.
 */
void global_cache_data::_process_pb_adaptive_host_status(
    const std::shared_ptr<io::data>& data) {
  const auto& in =
      std::static_pointer_cast<neb::pb_adaptive_host_status>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger, "cache: processing adaptive host status {}",
                      in.host_id());
  boost::unique_lock l(_protect);
  auto exist = _id_to_host->find(in.host_id());
  if (exist == _id_to_host->end() || !exist->second.first) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "Attempt to update host ({}) in cache, but it does not "
        "exist. Maybe Engine should be restarted to update the cache.",
        in.host_id());
    return;
  }
  auto& to_update = *exist->second.first;
  bool at_least_one_modif = false;
  UPDATE_OPTIONAL_FIELD(scheduled_downtime_depth);
  UPDATE_OPTIONAL_FIELD(acknowledgement_type);
  UPDATE_OPTIONAL_FIELD(notification_number);
  if (at_least_one_modif) {
    _set_dirty_and_increment_modif();
  }
}

/**
 *  Process a pb adaptive host event.
 *
 *  @param s  The event.
 */
void global_cache_data::_process_pb_adaptive_host(
    std::shared_ptr<io::data> const& data) {
  const auto& in = std::static_pointer_cast<neb::pb_adaptive_host>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger, "cache: processing adaptive host {}",
                      in.host_id());
  boost::unique_lock l(_protect);
  auto exist = _id_to_host->find(in.host_id());
  if (exist == _id_to_host->end() || !exist->second.first) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "Attempt to update host ({}) in cache, but it does not "
        "exist. Maybe Engine should be restarted to update the cache.",
        in.host_id());
    return;
  }
  auto& to_update = *exist->second.first;
  bool at_least_one_modif = false;
  UPDATE_OPTIONAL_FIELD(notify);
  UPDATE_OPTIONAL_FIELD(active_checks);
  UPDATE_OPTIONAL_FIELD(should_be_scheduled);
  UPDATE_OPTIONAL_FIELD(passive_checks);
  UPDATE_OPTIONAL_FIELD(event_handler_enabled);
  UPDATE_OPTIONAL_FIELD(flap_detection);
  UPDATE_OPTIONAL_FIELD(obsess_over_host);
  UPDATE_OPTIONAL_STRING_FIELD(event_handler);
  UPDATE_OPTIONAL_STRING_FIELD(check_command);
  UPDATE_OPTIONAL_FIELD(check_interval);
  UPDATE_OPTIONAL_FIELD(retry_interval);
  UPDATE_OPTIONAL_FIELD(max_check_attempts);
  UPDATE_OPTIONAL_FIELD(check_freshness);
  UPDATE_OPTIONAL_STRING_FIELD(check_period);
  UPDATE_OPTIONAL_STRING_FIELD(notification_period);
  if (at_least_one_modif) {
    _set_dirty_and_increment_modif();
  }
}

/**
 *  Process a host group event.
 *
 *  @param data  The event.
 */
void global_cache_data::_process_pb_host_group(
    std::shared_ptr<io::data> const& data) {
  const auto& in = std::static_pointer_cast<neb::pb_host_group>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "cache: processing pb host group '{}' of id {} for "
                      "poller {}, enabled {}",
                      in.name(), in.hostgroup_id(), in.poller_id(),
                      in.enabled());
  boost::unique_lock l(_protect);
  auto exist = _id_to_host_group->find(in.hostgroup_id());

  if (in.enabled()) {
    if (exist == _id_to_host_group->end()) {
      host_group* to_insert =
          _file->get_segment_manager()->construct<host_group>(
              interprocess::anonymous_instance)(in, *_allocators);
      _id_to_host_group->emplace(in.hostgroup_id(), to_insert);
      _set_dirty_and_increment_modif();
    } else if (exist->second->update(in, *_allocators)) {
      _set_dirty_and_increment_modif();
    }
  } else {
    size_t erased = _host_group_members->get<1>().erase(
        std::make_pair(in.hostgroup_id(), in.poller_id()));
    auto& hg_index = _host_group_members->get<0>();
    bool hg_erased = false;
    if (hg_index.find(in.hostgroup_id()) == hg_index.end()) {
      // no more host for this host group => remove
      if (exist != _id_to_host_group->end()) {
        _file->get_segment_manager()->destroy_ptr(exist->second.get());
        _id_to_host_group->erase(exist);
        hg_erased = true;
      }
    }
    if (hg_erased || erased > 0) {
      _set_dirty_and_increment_modif();
    }
  }
}

/**
 *  Process a host group member event.
 *
 *  @param data  The event.
 */
void global_cache_data::_process_pb_host_group_member(
    std::shared_ptr<io::data> const& data) {
  const auto& in =
      std::static_pointer_cast<neb::pb_host_group_member>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "cache: processing pb host group member (group_name: "
                      "'{}', group_id: {}, "
                      "host_id: {} poller_id: {}, enabled: {})",
                      in.name(), in.hostgroup_id(), in.host_id(),
                      in.poller_id(), in.enabled());

  boost::unique_lock l(_protect);
  if (in.enabled()) {
    bool dirty = false;
    HostGroup hg;
    hg.set_enabled(true);
    hg.set_hostgroup_id(in.hostgroup_id());
    hg.set_name(in.name());
    hg.set_poller_id(in.poller_id());
    auto hg_exist = _id_to_host_group->find(in.hostgroup_id());
    if (hg_exist == _id_to_host_group->end()) {
      host_group* to_insert =
          _file->get_segment_manager()->construct<host_group>(
              interprocess::anonymous_instance)(hg, *_allocators);
      _id_to_host_group->emplace(in.hostgroup_id(), to_insert);
      dirty = true;
    } else {
      if (hg.name().empty()) {
        hg.set_name(hg_exist->second->name().c_str());
      }
      dirty = hg_exist->second->update(hg, *_allocators);
    }
    dirty |= _host_group_members
                 ->emplace(in.host_id(), in.hostgroup_id(), in.poller_id())
                 .second;
    if (dirty) {
      _set_dirty_and_increment_modif();
    }
  } else {
    size_t erased = _host_group_members->get<3>().erase(
        host_group_member(in.host_id(), in.hostgroup_id(), in.poller_id()));
    // still members for this group
    auto& hg_index = _host_group_members->get<0>();
    bool hg_erased = false;
    if (hg_index.find(in.hostgroup_id()) == hg_index.end()) {
      // no more host for this host group => remove
      auto to_delete = _id_to_host_group->find(in.hostgroup_id());
      if (to_delete != _id_to_host_group->end()) {
        _file->get_segment_manager()->destroy_ptr(to_delete->second.get());
        _id_to_host_group->erase(to_delete);
        hg_erased = true;
      }
    }
    if (hg_erased || erased > 0) {
      _set_dirty_and_increment_modif();
    }
  }
}

/**
 *  Process a custom variable event.
 *  The goal is to keep in cache only custom variables concerning severity on
 *  hosts and services.
 *
 *  @param data  The event.
 */
void global_cache_data::_process_pb_custom_variable(
    std::shared_ptr<io::data> const& data) {
  const auto& in =
      std::static_pointer_cast<neb::pb_custom_variable>(data)->obj();
  if (in.name() == "CRITICALITY_LEVEL") {
    int32_t value;
    if (absl::SimpleAtoi(in.value(), &value)) {
      SPDLOG_LOGGER_TRACE(_logger,
                          "cache: processing custom variable representing a "
                          "criticality level for "
                          "host_id {} and service_id {} and level {}",
                          in.host_id(), in.service_id(), value);
      boost::unique_lock l(_protect);
      if (value) {
        if (in.service_id()) {
          auto exist = _id_to_service->find(
              std::make_pair(in.host_id(), in.service_id()));
          if (exist == _id_to_service->end()) {
            _id_to_service->emplace(
                host_serv_pair{in.host_id(), in.service_id()},
                service_custom_var_pair{nullptr, value});
            _set_dirty_and_increment_modif();
          } else {
            if (exist->second.second != value) {
              exist->second.second = value;
              _set_dirty_and_increment_modif();
            }
          }
        } else {
          auto exist = _id_to_host->find(in.host_id());
          if (exist == _id_to_host->end()) {
            _id_to_host->emplace(in.host_id(),
                                 host_custom_var_pair{nullptr, value});
            _set_dirty_and_increment_modif();
          } else {
            if (exist->second.second != value) {
              exist->second.second = value;
              _set_dirty_and_increment_modif();
            }
          }
        }
      }
    } else {
      SPDLOG_LOGGER_ERROR(_logger,
                          "cache "
                          "criticality level for "
                          "host_id {} and service_id {} incorrect value {}",
                          in.host_id(), in.service_id(), in.value());
    }
  }
}

/**
 *  Process a pb service event.
 *
 *  @param s  The event.
 */
void global_cache_data::_process_pb_service(
    std::shared_ptr<io::data> const& data) {
  const auto& in = std::static_pointer_cast<neb::pb_service>(data)->obj();
  SPDLOG_LOGGER_TRACE(
      _logger, "cache: processing service ({}, {}) (description:{}) enabled {}",
      in.host_id(), in.service_id(), in.description(), in.enabled());
  boost::unique_lock l(_protect);
  auto exist =
      _id_to_service->find(std::make_pair(in.host_id(), in.service_id()));
  if (exist == _id_to_service->end()) {
    if (in.enabled()) {
      service* to_insert = _file->get_segment_manager()->construct<service>(
          interprocess::anonymous_instance)(in, *_allocators);
      _id_to_service->emplace(host_serv_pair{in.host_id(), in.service_id()},
                              service_custom_var_pair{to_insert, 0});
      _set_dirty_and_increment_modif();
    }
  } else {
    if (in.enabled()) {
      if (!exist->second.first) {
        exist->second.first = _file->get_segment_manager()->construct<service>(
            interprocess::anonymous_instance)(in, *_allocators);
        _set_dirty_and_increment_modif();
      } else if (exist->second.first->update(in, *_allocators)) {
        _set_dirty_and_increment_modif();
      }
    } else {
      if (exist->second.first) {
        _file->get_segment_manager()->destroy_ptr(exist->second.first.get());
      }
      _id_to_service->erase(exist);
      _set_dirty_and_increment_modif();
    }
  }
}

/**
 * @brief Process a pb service status event.
 *
 * @param data
 */
void global_cache_data::_process_pb_service_status(
    const std::shared_ptr<io::data>& data) {
  const auto& in =
      std::static_pointer_cast<neb::pb_service_status>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger, "cache: processing host status {}",
                      in.host_id());
  boost::unique_lock l(_protect);
  auto exist = _id_to_service->find({in.host_id(), in.service_id()});
  if (exist == _id_to_service->end() || !exist->second.first) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "Attempt to update service ({}, {}) in cache, but it does not "
        "exist. Maybe Engine should be restarted to update the cache.",
        in.host_id(), in.service_id());
    return;
  }
  auto& to_update = *exist->second.first;
  bool at_least_one_modif = false;
  UPDATE_FIELD(checked);
  UPDATE_FIELD(check_type);
  UPDATE_FIELD(state);
  UPDATE_FIELD(state_type);
  UPDATE_FIELD(last_state_change);
  UPDATE_FIELD(last_hard_state);
  UPDATE_FIELD(last_hard_state_change);
  UPDATE_FIELD(last_time_ok);
  UPDATE_FIELD(last_time_warning);
  UPDATE_FIELD(last_time_critical);
  UPDATE_FIELD(last_time_unknown);
  UPDATE_STRING_FIELD(output);
  UPDATE_STRING_FIELD(perfdata);
  UPDATE_FIELD(flapping);
  UPDATE_FIELD(percent_state_change);
  UPDATE_FIELD(latency);
  UPDATE_FIELD(execution_time);
  UPDATE_FIELD(last_check);
  UPDATE_FIELD(next_check);
  UPDATE_FIELD(should_be_scheduled);
  UPDATE_FIELD(check_attempt);
  UPDATE_FIELD(notification_number);
  UPDATE_FIELD(no_more_notifications);
  UPDATE_FIELD(last_notification);
  UPDATE_FIELD(next_notification);
  UPDATE_FIELD(acknowledgement_type);
  UPDATE_FIELD(scheduled_downtime_depth);
  if (at_least_one_modif) {
    _set_dirty_and_increment_modif();
  }
}

/**
 * @brief Process a pb adaptive service status event.
 *
 * @param data An AdaptiveServiceStatus event.
 */
void global_cache_data::_process_pb_adaptive_service_status(
    const std::shared_ptr<io::data>& data) {
  const auto& in =
      std::static_pointer_cast<neb::pb_adaptive_service_status>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger, "cache: processing adaptive service status {}",
                      in.host_id());
  boost::unique_lock l(_protect);
  auto exist = _id_to_service->find({in.host_id(), in.service_id()});
  if (exist == _id_to_service->end() || !exist->second.first) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "Attempt to update service ({}, {}) in cache, but it does not "
        "exist. Maybe Engine should be restarted to update the cache.",
        in.host_id(), in.service_id());
    return;
  }
  auto& to_update = *exist->second.first;
  bool at_least_one_modif = false;
  UPDATE_OPTIONAL_FIELD(scheduled_downtime_depth);
  UPDATE_OPTIONAL_FIELD(acknowledgement_type);
  UPDATE_OPTIONAL_FIELD(notification_number);

  if (at_least_one_modif) {
    _set_dirty_and_increment_modif();
  }
}

/**
 * @brief Process a pb adaptive service event.
 *
 * @param data An AdaptiveService event.
 */
void global_cache_data::_process_pb_adaptive_service(
    std::shared_ptr<io::data> const& data) {
  const auto& in =
      std::static_pointer_cast<neb::pb_adaptive_service>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger, "cache: processing adaptive service status {}",
                      in.host_id());
  boost::unique_lock l(_protect);
  auto exist = _id_to_service->find({in.host_id(), in.service_id()});
  if (exist == _id_to_service->end() || !exist->second.first) {
    SPDLOG_LOGGER_WARN(
        _logger,
        "Attempt to update service ({}, {}) in cache, but it does not "
        "exist. Maybe Engine should be restarted to update the cache.",
        in.host_id(), in.service_id());
    return;
  }
  auto& to_update = *exist->second.first;
  bool at_least_one_modif = false;

  UPDATE_OPTIONAL_FIELD(notify);
  UPDATE_OPTIONAL_FIELD(active_checks);
  UPDATE_OPTIONAL_FIELD(should_be_scheduled);
  UPDATE_OPTIONAL_FIELD(passive_checks);
  UPDATE_OPTIONAL_FIELD(event_handler_enabled);
  if (in.has_flap_detection_enabled() &&
      to_update.flap_detection() != in.flap_detection_enabled()) {
    to_update.mutable_flap_detection() = in.flap_detection_enabled();
    at_least_one_modif = true;
  }
  UPDATE_OPTIONAL_FIELD(obsess_over_service);
  UPDATE_OPTIONAL_STRING_FIELD(event_handler);
  UPDATE_OPTIONAL_STRING_FIELD(check_command);
  UPDATE_OPTIONAL_FIELD(check_interval);
  UPDATE_OPTIONAL_FIELD(retry_interval);
  UPDATE_OPTIONAL_FIELD(max_check_attempts);
  UPDATE_OPTIONAL_FIELD(check_freshness);
  UPDATE_OPTIONAL_STRING_FIELD(check_period);
  UPDATE_OPTIONAL_STRING_FIELD(notification_period);
  if (at_least_one_modif) {
    _set_dirty_and_increment_modif();
  }
}

/**
 *  Process a service group event.
 *
 *  @param data  The event.
 */
void global_cache_data::_process_pb_service_group(
    std::shared_ptr<io::data> const& data) {
  const auto& in = std::static_pointer_cast<neb::pb_service_group>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "cache: processing pb service group '{}' of id {}",
                      in.name(), in.servicegroup_id());
  boost::unique_lock l(_protect);
  auto exist = _id_to_serv_group->find(in.servicegroup_id());

  if (in.enabled()) {
    if (exist == _id_to_serv_group->end()) {
      service_group* to_insert =
          _file->get_segment_manager()->construct<service_group>(
              interprocess::anonymous_instance)(in, *_allocators);
      _id_to_serv_group->emplace(in.servicegroup_id(), to_insert);
      _set_dirty_and_increment_modif();
    } else if (exist->second->update(in, *_allocators)) {
      _set_dirty_and_increment_modif();
    }
  } else {
    size_t erased = _service_group_members->get<1>().erase(
        std::make_pair(in.servicegroup_id(), in.poller_id()));
    auto& sg_index = _service_group_members->get<0>();
    bool sg_erased = false;
    if (sg_index.find(in.servicegroup_id()) == sg_index.end()) {
      // no more service for this service group => remove
      if (exist != _id_to_serv_group->end()) {
        _file->get_segment_manager()->destroy_ptr(exist->second.get());
        _id_to_serv_group->erase(exist);
        sg_erased = true;
      }
    }
    if (sg_erased || erased > 0) {
      _set_dirty_and_increment_modif();
    }
  }
}

/**
 *  Process a service group member event.
 *
 *  @param data  The event.
 */
void global_cache_data::_process_pb_service_group_member(
    std::shared_ptr<io::data> const& data) {
  const auto& in =
      std::static_pointer_cast<neb::pb_service_group_member>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "cache: processing pb service group member (group_name: "
                      "'{}', group_id: {}, "
                      "host_id: {}, service_id: {} poller_id: {}, enabled: {})",
                      in.name(), in.servicegroup_id(), in.host_id(),
                      in.service_id(), in.poller_id(), in.enabled());

  boost::unique_lock l(_protect);
  if (in.enabled()) {
    bool dirty = false;
    ServiceGroup sg;
    sg.set_enabled(true);
    sg.set_servicegroup_id(in.servicegroup_id());
    sg.set_name(in.name());
    sg.set_poller_id(in.poller_id());
    auto sg_exist = _id_to_serv_group->find(in.servicegroup_id());
    if (sg_exist == _id_to_serv_group->end()) {
      service_group* to_insert =
          _file->get_segment_manager()->construct<service_group>(
              interprocess::anonymous_instance)(sg, *_allocators);
      _id_to_serv_group->emplace(in.servicegroup_id(), to_insert);
      dirty = true;
    } else {
      if (sg.name().empty()) {
        sg.set_name(sg_exist->second->name().c_str());
      }
      dirty = sg_exist->second->update(sg, *_allocators);
    }
    dirty |= _service_group_members
                 ->emplace(in.host_id(), in.service_id(), in.servicegroup_id(),
                           in.poller_id())
                 .second;
    if (dirty) {
      _set_dirty_and_increment_modif();
    }
  } else {
    size_t erased = _service_group_members->get<3>().erase(service_group_member(
        in.host_id(), in.service_id(), in.servicegroup_id(), in.poller_id()));
    // still members for this group
    auto& sg_index = _service_group_members->get<0>();
    bool sg_erased = false;
    if (sg_index.find(in.servicegroup_id()) == sg_index.end()) {
      // no more service for this service group => remove
      auto to_delete = _id_to_serv_group->find(in.servicegroup_id());
      if (to_delete != _id_to_serv_group->end()) {
        _file->get_segment_manager()->destroy_ptr(to_delete->second.get());
        _id_to_serv_group->erase(to_delete);
        sg_erased = true;
      }
    }
    if (sg_erased || erased > 0) {
      _set_dirty_and_increment_modif();
    }
  }
}

/**
 *  Process an instance event.
 *
 *  @param in  The event.
 */
void global_cache_data::_process_pb_tag(std::shared_ptr<io::data> const& data) {
  const auto& in = std::static_pointer_cast<neb::pb_tag>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "cache: processing pb tag (name: '{}', id: {}, type: {})",
                      in.name(), in.id(), in.type());
  boost::unique_lock l(_protect);
  auto key = std::make_pair(in.id(), in.type());
  auto exist = _id_to_tag->find(key);
  if (exist == _id_to_tag->end()) {
    if (in.action() != Tag::DELETE) {
      tag* to_insert = _file->get_segment_manager()->construct<tag>(
          interprocess::anonymous_instance)(in, *_allocators);
      _id_to_tag->emplace(key, tag_poller(to_insert, *_allocators));
      _set_dirty_and_increment_modif();
    }
  } else {
    if (in.action() != Tag::DELETE) {
      if (exist->second.data->update(in, *_allocators)) {
        _set_dirty_and_increment_modif();
      }
      if (exist->second.pollers.insert(in.poller_id()).second) {
        _set_dirty_and_increment_modif();
      }
    } else {
      exist->second.pollers.erase(in.poller_id());
      if (exist->second.pollers.empty()) {
        _file->get_segment_manager()->destroy_ptr(exist->second.data.get());
        _id_to_tag->erase(exist);
      }
      _set_dirty_and_increment_modif();
    }
  }
}

/**
 *  Process an index mapping event.
 *
 *  @param im  The event.
 */
void global_cache_data::_process_index_mapping(
    std::shared_ptr<io::data> const& data) {
  const auto& in =
      std::static_pointer_cast<storage::pb_index_mapping>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "cache: processing pb index mapping (index_id: '{}', "
                      "host_id: {}, service_id: {})",
                      in.index_id(), in.host_id(), in.service_id());
  boost::unique_lock l(_protect);
  auto exist = _index_id_mapping->find(in.index_id());
  host_serv_pair host_serv_id{in.host_id(), in.service_id()};
  if (exist == _index_id_mapping->end()) {
    _index_id_mapping->emplace(in.index_id(), host_serv_id);
    _set_dirty_and_increment_modif();
  } else if (exist->second != host_serv_id) {
    exist->second = host_serv_id;
    _set_dirty_and_increment_modif();
  }
}

/**
 *  Process a metric mapping event.
 *
 *  @param data  The event.
 */
void global_cache_data::_process_metric_mapping(
    std::shared_ptr<io::data> const& data) {
  const auto& in =
      std::static_pointer_cast<storage::pb_metric_mapping>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "cache: processing pb metric mapping (index_id: '{}', "
                      "metric_id: {})",
                      in.index_id(), in.metric_id());

  boost::unique_lock l(_protect);
  auto exist = _metric_id_mapping->find(in.metric_id());
  if (exist == _metric_id_mapping->end()) {
    _metric_id_mapping->emplace(in.metric_id(), in.index_id());
    _set_dirty_and_increment_modif();
  } else if (exist->second != in.index_id()) {
    exist->second = in.index_id();
    _set_dirty_and_increment_modif();
  }
}

/**
 *  Process a dimension ba event
 *
 *  @param data  The event.
 */
void global_cache_data::_process_dimension_ba_event(
    std::shared_ptr<io::data> const& data) {
  const auto& in =
      std::static_pointer_cast<bam::pb_dimension_ba_event>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "cache: processing pb dimension ba event (ba_id: '{}', "
                      "name: {})",
                      in.ba_id(), in.ba_name());
  boost::unique_lock l(_protect);
  auto exist = _id_to_dimension_ba_event->find(in.ba_id());
  if (exist == _id_to_dimension_ba_event->end()) {
    auto* to_insert =
        _file->get_segment_manager()->construct<dimension_ba_event>(
            interprocess::anonymous_instance)(in, *_allocators);
    _id_to_dimension_ba_event->emplace(in.ba_id(), to_insert);
    _set_dirty_and_increment_modif();
  } else if (exist->second->update(in, *_allocators)) {
    _set_dirty_and_increment_modif();
  }
}

void global_cache_data::_process_dimension_ba_bv_relation_event(
    std::shared_ptr<io::data> const& data) {
  const auto& in =
      std::static_pointer_cast<bam::pb_dimension_ba_bv_relation_event>(data)
          ->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "cache: processing pb dimension ba bv relation event "
                      "(ba_id: {}, bv_id: {})",
                      in.ba_id(), in.bv_id());
  boost::unique_lock l(_protect);
  auto exist = _id_to_dimension_ba_bv_relation->equal_range(in.ba_id());
  bool found = false;
  for (; exist.first != exist.second; ++exist.first) {
    if (exist.first->second == in.bv_id()) {
      found = true;
      break;
    }
  }
  if (!found) {
    _id_to_dimension_ba_bv_relation->emplace(in.ba_id(), in.bv_id());
    _set_dirty_and_increment_modif();
  }
}

/**
 *  Process a dimension bv event
 *
 *  @param data  The event.
 */
void global_cache_data::_process_dimension_bv_event(
    std::shared_ptr<io::data> const& data) {
  const auto& in =
      std::static_pointer_cast<bam::pb_dimension_bv_event>(data)->obj();
  SPDLOG_LOGGER_TRACE(_logger,
                      "cache: processing pb dimension ba event (ba_id: '{}', "
                      "name: {})",
                      in.bv_id(), in.bv_name());
  boost::unique_lock l(_protect);
  auto exist = _id_to_dimension_bv_event->find(in.bv_id());
  if (exist == _id_to_dimension_bv_event->end()) {
    auto* to_insert =
        _file->get_segment_manager()->construct<dimension_bv_event>(
            interprocess::anonymous_instance)(in, *_allocators);
    _id_to_dimension_bv_event->emplace(in.bv_id(), to_insert);
    _set_dirty_and_increment_modif();
  } else if (exist->second->update(in, *_allocators)) {
    _set_dirty_and_increment_modif();
  }
}

/**
 *  Process a dimension truncate table signal
 *
 * @param data  The event.
 */
void global_cache_data::_process_pb_dimension_truncate_table_signal(
    std::shared_ptr<io::data> const& data) {
  SPDLOG_LOGGER_TRACE(_logger,
                      "lua: processing dimension truncate table signal");

  if (std::static_pointer_cast<bam::pb_dimension_truncate_table_signal>(data)
          ->obj()
          .update_started()) {
    boost::unique_lock l(_protect);
    bool cleared = !_id_to_dimension_ba_event->empty() ||
                   !_id_to_dimension_bv_event->empty() ||
                   !_id_to_dimension_ba_bv_relation->empty();
    if (cleared) {
      _id_to_dimension_ba_event->clear();
      _id_to_dimension_ba_bv_relation->clear();
      _id_to_dimension_bv_event->clear();
      _set_dirty_and_increment_modif();
    }
  }
}

/**
 * @brief get host from cache
 *
 * @param host_id
 * @return const host* nullptr if not found
 */
global_cache_data::host_custom_var_pair global_cache_data::_get_host(
    uint64_t host_id,
    lock& l) {
  l.upgrade_lock(&_protect);
  auto search = _id_to_host->find(host_id);
  if ((search == _id_to_host->end() || !search->second.first) &&
      _cache_type == e_cache_type::real_time &&
      _conf_cache) {  // not found in rt cache => search in conf cache
    lock conf_lock;
    auto conf_host_sev =
        std::static_pointer_cast<global_cache_data>(_conf_cache)
            ->_get_host(host_id, conf_lock);
    if (conf_host_sev.first) {
      l.upgrade_to_unique();
      host* to_insert = _file->get_segment_manager()->construct<host>(
          interprocess::anonymous_instance)(*conf_host_sev.first, *_allocators);
      _id_to_host->emplace(
          host_id, host_custom_var_pair(to_insert, conf_host_sev.second));
      _set_dirty_and_increment_modif();
      l.unique_to_shared();
      return std::make_pair(to_insert, conf_host_sev.second);
    }
  }
  return search != _id_to_host->end() ? search->second
                                      : host_custom_var_pair{nullptr, 0};
}

/**
 * @brief get host from cache
 *
 * @param host_id
 * @return const host* nullptr if not found
 */
const host* global_cache_data::get_host(uint64_t host_id, lock& l) {
  return _get_host(host_id, l).first.get();
}

/**
 * @brief get service from cache
 *
 * @param host_id
 * @param service_id
 * @return const service* nullptr if not found
 */
global_cache_data::service_custom_var_pair global_cache_data::_get_service(
    uint64_t host_id,
    uint64_t service_id,
    lock& l) {
  l.upgrade_lock(&_protect);
  auto search = _id_to_service->find({host_id, service_id});
  if ((search == _id_to_service->end() || !search->second.first) &&
      _cache_type == e_cache_type::real_time &&
      _conf_cache) {  // not found in rt cache => search in conf cache
    lock conf_lock;
    service_custom_var_pair conf_service =
        std::static_pointer_cast<global_cache_data>(_conf_cache)
            ->_get_service(host_id, service_id, conf_lock);
    if (conf_service.first) {
      l.upgrade_to_unique();
      service* to_insert = _file->get_segment_manager()->construct<service>(
          interprocess::anonymous_instance)(*conf_service.first, *_allocators);
      _id_to_service->emplace(
          std::make_pair(host_id, service_id),
          service_custom_var_pair(to_insert, conf_service.second));
      _set_dirty_and_increment_modif();
      l.unique_to_shared();
      return std::make_pair(to_insert, conf_service.second);
    }
  }
  return search != _id_to_service->end() ? search->second
                                         : service_custom_var_pair{nullptr, 0};
}

/**
 * @brief get service from cache
 *
 * @param host_id
 * @param service_id
 * @return const service* nullptr if not found
 */
const service* global_cache_data::get_service(uint64_t host_id,
                                              uint64_t service_id,
                                              lock& l) {
  return _get_service(host_id, service_id, l).first.get();
}

/**
 * @brief get host_id and service_id from index_id
 *
 * @param index_id
 * @return const host_serv_pair* nullptr if not found
 */
std::optional<host_serv_pair> global_cache_data::get_host_serv_id(
    uint64_t index_id) {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    return _conf_cache->get_host_serv_id(index_id);
  }
  boost::shared_lock l(_protect);
  auto search = _index_id_mapping->find(index_id);
  if (search != _index_id_mapping->end()) {
    return search->second;
  } else {
    return {};
  }
}

/**
 * @brief get instance info from instance_id
 *
 * @param instance_id
 * @return const instance* nullptr if not found
 */
const instance* global_cache_data::get_instance(uint64_t instance_id, lock& l) {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    return _conf_cache->get_instance(instance_id, l);
  }
  l.shared_lock(&_protect);
  auto search = _id_to_instance->find(instance_id);
  return search != _id_to_instance->end() ? search->second.get() : nullptr;
}

/**
 * @brief get host_group from id
 *
 * @param group_id
 * @return const host_group* null if not found
 */
const host_group* global_cache_data::get_host_group(uint64_t group_id,
                                                    lock& l) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    return _conf_cache->get_host_group(group_id, l);
  }
  l.shared_lock(&_protect);
  auto search = _id_to_host_group->find(group_id);
  return search != _id_to_host_group->end() ? search->second.get() : nullptr;
}

/**
 * @brief get service_group from id
 *
 * @param group_id
 * @return const service_group* null if not found
 */
const service_group* global_cache_data::get_service_group(uint64_t group_id,
                                                          lock& l) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    return _conf_cache->get_service_group(group_id, l);
  }
  l.shared_lock(&_protect);
  auto search = _id_to_serv_group->find(group_id);
  return search != _id_to_serv_group->end() ? search->second.get() : nullptr;
}

static void sorted_id_to_str(const std::set<uint64_t>& ids,
                             std::ostream& request_body) {
  if (!ids.empty()) {
    std::set<uint64_t>::const_iterator val_iter = ids.begin();
    request_body << *(val_iter++);
    for (; val_iter != ids.end(); ++val_iter) {
      request_body << ',' << *val_iter;
    }
  }
}

/**
 * @brief add service groups to a request body
 *
 * @param host
 * @param service
 * @param request_body
 */
void global_cache_data::append_service_group(uint64_t host,
                                             uint64_t service,
                                             std::ostream& request_body) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    _conf_cache->append_service_group(host, service, request_body);
    return;
  }

  std::set<uint64_t> sorted;
  {
    boost::shared_lock l(_protect);
    auto range = _service_group_members->get<2>().equal_range(
        host_serv_pair{host, service});
    if (range.first != range.second) {
      // in order to avoid cardinality, we sort results
      for (; range.first != range.second; ++range.first) {
        sorted.insert(range.first->group_id);
      }
    }
  }
  sorted_id_to_str(sorted, request_body);
}

/**
 * @brief append host groups to a request body
 *
 * @param host
 * @param request_body
 */
void global_cache_data::append_host_group(uint64_t host,
                                          std::ostream& request_body) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    _conf_cache->append_host_group(host, request_body);
    return;
  }

  std::set<uint64_t> sorted;
  {
    boost::shared_lock l(_protect);
    auto range = _host_group_members->get<2>().equal_range(host);
    if (range.first != range.second) {
      // in order to avoid cardinality, we sort results
      for (; range.first != range.second; ++range.first) {
        sorted.insert(range.first->group_id);
      }
    }
  }
  sorted_id_to_str(sorted, request_body);
}

/**
 * @brief append tag ids of a host to a request body
 *
 * @param host
 * @param tag_type
 * @param request_body
 */
void global_cache_data::append_host_tag_id(uint64_t host,
                                           TagType tag_type,
                                           std::ostream& request_body) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    _conf_cache->append_host_tag_id(host, tag_type, request_body);
    return;
  }

  boost::shared_lock l(_protect);
  auto search = _id_to_host->find(host);

  if (search != _id_to_host->end() && search->second.first) {
    std::set<uint64_t> sorted;
    for (const auto& tag : search->second.first->tags()) {
      const tag_info& tag_inf = *static_cast<const tag_info*>(tag.get());
      if (tag_inf.type() == tag_type) {
        sorted.insert(tag_inf.id());
      }
    }
    sorted_id_to_str(sorted, request_body);
  }
}

/**
 * @brief append tag ids of a service to a request body
 *
 * @param host
 * @param serv
 * @param tag_type
 * @param request_body
 */
void global_cache_data::append_serv_tag_id(uint64_t host,
                                           uint64_t serv,
                                           TagType tag_type,
                                           std::ostream& request_body) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    _conf_cache->append_serv_tag_id(host, serv, tag_type, request_body);
    return;
  }

  boost::shared_lock l(_protect);
  auto search = _id_to_service->find({host, serv});
  if (search != _id_to_service->end() && search->second.first) {
    std::set<uint64_t> sorted;
    for (const auto& tag : search->second.first->tags()) {
      const tag_info& tag_inf = *static_cast<const tag_info*>(tag.get());
      if (tag_inf.type() == tag_type) {
        sorted.insert(tag_inf.id());
      }
    }
    sorted_id_to_str(sorted, request_body);
  }
}

/**
 * @brief append tag names of a host to a request body
 *
 * @param host
 * @param tag_type
 * @param request_body
 */
void global_cache_data::append_host_tag_name(uint64_t host,
                                             TagType tag_type,
                                             std::ostream& request_body) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    _conf_cache->append_host_tag_name(host, tag_type, request_body);
    return;
  }
  boost::shared_lock l(_protect);
  auto search = _id_to_host->find(host);

  if (search != _id_to_host->end() && search->second.first) {
    bool first = true;
    for (const auto& tag : search->second.first->tags()) {
      const tag_info& tag_inf = *static_cast<const tag_info*>(tag.get());
      if (tag_inf.type() == tag_type) {
        auto tag_search =
            _id_to_tag->find(std::make_pair(tag_inf.id(), tag_type));
        if (tag_search != _id_to_tag->end()) {
          if (first) {
            request_body << tag_search->second.data->name();
            first = false;
          } else {
            request_body << ',' << tag_search->second.data->name();
          }
        }
      }
    }
  }
}

/**
 * @brief append tag names of a service to a request body
 *
 * @param host
 * @param serv
 * @param tag_type
 * @param request_body
 */
void global_cache_data::append_serv_tag_name(uint64_t host,
                                             uint64_t serv,
                                             TagType tag_type,
                                             std::ostream& request_body) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    _conf_cache->append_serv_tag_name(host, serv, tag_type, request_body);
    return;
  }
  boost::shared_lock l(_protect);
  auto search = _id_to_service->find({host, serv});
  if (search != _id_to_service->end() && search->second.first) {
    bool first = true;
    for (const auto& tag : search->second.first->tags()) {
      const tag_info& tag_inf = *static_cast<const tag_info*>(tag.get());
      if (tag_inf.type() == tag_type) {
        auto tag_search =
            _id_to_tag->find(std::make_pair(tag_inf.id(), tag_type));
        if (tag_search != _id_to_tag->end()) {
          if (first) {
            request_body << tag_search->second.data->name();
            first = false;
          } else {
            request_body << ',' << tag_search->second.data->name();
          }
        }
      }
    }
  }
}

/**
 * @brief get index_id from metric_id
 *
 * @param metric_id
 * @return uint64_t 0 if not found
 */
uint64_t global_cache_data::get_index_id_from_metric_id(
    uint64_t metric_id) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    return _conf_cache->get_index_id_from_metric_id(metric_id);
  }
  boost::shared_lock l(_protect);
  auto search = _metric_id_mapping->find(metric_id);
  return search != _metric_id_mapping->end() ? search->second : 0;
}

/**
 * @brief
 *
 * @param host_id
 * @param service_id 0 for host
 * @return int32_t
 */
std::optional<int32_t> global_cache_data::get_severity(
    uint64_t host_id,
    uint64_t service_id) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    return _conf_cache->get_severity(host_id, service_id);
  }
  boost::shared_lock l(_protect);
  if (service_id) {
    auto search = _id_to_service->find({host_id, service_id});
    if (search != _id_to_service->end()) {
      return search->second.second;
    } else {
      return {};
    }
  } else {
    auto search = _id_to_host->find(host_id);
    if (search != _id_to_host->end()) {
      return search->second.second;
    } else {
      return {};
    }
  }
}

/**
 *  Return a dimension_ba_event from its id.
 *
 * @param ba_id The id
 *
 * @return a pointer to the dimension_ba_event.
 */
const dimension_ba_event* global_cache_data::get_dimension_ba_event(
    uint64_t ba_id,
    lock& l) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    return _conf_cache->get_dimension_ba_event(ba_id, l);
  }
  l.shared_lock(&_protect);
  auto search = _id_to_dimension_ba_event->find(ba_id);
  return search != _id_to_dimension_ba_event->end() ? search->second.get()
                                                    : nullptr;
}

/**
 *  Return a dimension_bv_event from its id.
 *
 * @param ba_id The id
 *
 * @return a pointer to the dimension_bv_event.
 */
const dimension_bv_event* global_cache_data::get_dimension_bv_event(
    uint64_t bv_id,
    lock& l) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    return _conf_cache->get_dimension_bv_event(bv_id, l);
  }
  l.shared_lock(&_protect);
  auto search = _id_to_dimension_bv_event->find(bv_id);
  return search != _id_to_dimension_bv_event->end() ? search->second.get()
                                                    : nullptr;
}

/**
 * @brief call enumerator for each bv of ba
 *
 * @param ba_id id of the ba
 * @param enumerator function called for each bv found
 */
void global_cache_data::enumerate_bvs(uint64_t ba_id,
                                      bv_enumerator&& enumerator) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    _conf_cache->enumerate_bvs(ba_id, std::move(enumerator));
    return;
  }
  boost::shared_lock l(_protect);
  auto bvs = _id_to_dimension_ba_bv_relation->equal_range(ba_id);
  for (; bvs.first != bvs.second; ++bvs.first) {
    enumerator(bvs.first->second);
  }
}

/**
 * @brief call enumerator for each host group of the host
 *
 * @param host_id
 * @param enumerator function called
 */
void global_cache_data::enumerate_host_group(
    uint64_t host_id,
    group_enumerator&& enumerator) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    _conf_cache->enumerate_host_group(host_id, std::move(enumerator));
    return;
  }
  boost::shared_lock l(_protect);
  auto group_member_search = _host_group_members->get<2>().equal_range(host_id);
  for (; group_member_search.first != group_member_search.second;
       ++group_member_search.first) {
    auto grp_search =
        _id_to_host_group->find(group_member_search.first->group_id);
    if (grp_search != _id_to_host_group->end()) {
      enumerator(grp_search->first, grp_search->second->name());
    }
  }
}

/**
 * @brief call enumerator for each host group of the service
 *
 * @param host_id
 * @param service_id
 * @param enumerator
 */
void global_cache_data::enumerate_service_group(
    uint64_t host_id,
    uint64_t service_id,
    group_enumerator&& enumerator) const {
  if (_cache_type == e_cache_type::real_time &&
      _conf_cache) {  // pure conf => we search in conf cache
    _conf_cache->enumerate_service_group(host_id, service_id,
                                         std::move(enumerator));
    return;
  }

  boost::shared_lock l(_protect);
  auto group_member_search = _service_group_members->get<2>().equal_range(
      std::make_pair(host_id, service_id));
  for (; group_member_search.first != group_member_search.second;
       ++group_member_search.first) {
    auto grp_search =
        _id_to_serv_group->find(group_member_search.first->group_id);
    if (grp_search != _id_to_serv_group->end()) {
      enumerator(grp_search->first, grp_search->second->name());
    }
  }
}
