/**
 * Copyright 2020-2024 Centreon
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

#include "com/centreon/broker/stats/center.hh"

#include <absl/synchronization/mutex.h>
#include <google/protobuf/util/json_util.h>

#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/misc/filesystem.hh"
#include "com/centreon/broker/version.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::stats;
using namespace google::protobuf::util;
using namespace com::centreon::broker::modules;
using com::centreon::common::log_v2::log_v2;

std::shared_ptr<center> center::_instance;

std::shared_ptr<center> center::instance_ptr() {
  assert(_instance);
  return _instance;
}

void center::load() {
  if (!_instance)
    _instance = std::make_shared<center>();
}

void center::unload() {
  _instance.reset();
}

center::center() {
  *_stats.mutable_version() = version::string;
  *_stats.mutable_asio_version() =
      fmt::format("{}.{}.{}", BOOST_ASIO_VERSION / 100000,
                  BOOST_ASIO_VERSION / 100 % 1000, BOOST_ASIO_VERSION % 100);
  _stats.set_pid(getpid());

  /* Bringing modules statistics */
  if (config::applier::state::loaded()) {
    config::applier::modules& mod_applier(
        config::applier::state::instance().get_modules());
    for (config::applier::modules::iterator it = mod_applier.begin(),
                                            end = mod_applier.end();
         it != end; ++it) {
      auto m = _stats.add_modules();
      *m->mutable_name() = it->first;
      *m->mutable_size() =
          fmt::format("{}B", misc::filesystem::file_size(it->first));
      *m->mutable_state() = "loaded";
    }
  }
}

/**
 * @brief If the engine needs to write statistics, it primarily has to
 * call this function to be registered in the statistic center and to get
 * a pointer for its statistics. It is prohibited to directly write into this
 * pointer. We must use the center member functions for this purpose.
 *
 * @param name
 *
 * @return A pointer to the engine statistics.
 */
EngineStats* center::register_engine() {
  absl::MutexLock lck(&_stats_m);
  return _stats.mutable_processing()->mutable_engine();
}

SqlConnectionStats* center::connection(size_t idx) {
  return &_stats.mutable_sql_manager()->mutable_connections()->at(idx);
}

/**
 * @brief Add a new connection stats entry and returns its index.
 *
 * @return A pointer to the connection statistics.
 */
SqlConnectionStats* center::add_connection() {
  absl::MutexLock lck(&_stats_m);
  return _stats.mutable_sql_manager()->add_connections();
}

void center::remove_connection(SqlConnectionStats* stats) {
  absl::MutexLock lck(&_stats_m);
  auto* c = _stats.mutable_sql_manager()->mutable_connections();
  for (auto it = c->begin(); it != c->end(); ++it) {
    if (&*it == stats) {
      c->erase(it);
      break;
    }
  }
}

/**
 * @brief Unregister a muxer from the stats. It removed its statistics from
 * the center statistics. In case of updates not already set to the stats,
 * the function waits for them before unregistering.
 *
 * @param name The name of the concerned muxer.
 *
 * @return true on success (it never fails).
 */
void center::unregister_muxer(const std::string& name) {
  absl::MutexLock lck(&_stats_m);
  _stats.mutable_processing()->mutable_muxers()->erase(name);
}

/**
 * @brief Update the given muxer statistics in the center.
 *
 * @param name The name of the concerned muxer.
 * @param queue_file its queue file
 * @param size current total events.
 * @param unack current unacknowledged events.
 */
void center::update_muxer(std::string name,
                          std::string queue_file,
                          uint32_t size,
                          uint32_t unack) {
  absl::MutexLock lck(&_stats_m);
  auto ms = &(*_stats.mutable_processing()->mutable_muxers())[std::move(name)];
  if (ms) {
    ms->mutable_queue_file()->set_name(std::move(queue_file));
    ms->set_total_events(size);
    ms->set_unacknowledged_events(unack);
  }
}

void center::init_queue_file(std::string muxer,
                             std::string queue_file,
                             uint32_t max_file_size) {
  absl::MutexLock lck(&_stats_m);
  auto qfs =
      (&(*_stats.mutable_processing()->mutable_muxers())[std::move(muxer)])
          ->mutable_queue_file();
  if (qfs) {
    qfs->set_name(std::move(queue_file));
    qfs->set_max_file_size(max_file_size);
  }
}

/**
 * @brief To allow the conflict manager to send statistics, it has to call this
 * function to get a pointer to its statistics container.
 * It is prohibited to directly write into the returned pointer. We must use
 * the center member functions for this purpose.
 *
 * @return A pointer to the conflict_manager statistics.
 */
ConflictManagerStats* center::register_conflict_manager() {
  absl::MutexLock lck(&_stats_m);
  return _stats.mutable_conflict_manager();
}

/**
 * @brief Convert the protobuf statistics object to a json string.
 *
 * @return a string with the object in json format.
 */
std::string center::to_string() {
  const JsonPrintOptions options;
  std::string retval;
  std::time_t now = time(nullptr);
  absl::MutexLock lck(&_stats_m);
  _json_stats_file_creation = now;
  _stats.set_now(now);
  auto status = MessageToJsonString(_stats, &retval, options);
  return retval;
}

void center::get_sql_manager_stats(SqlManagerStats* response, int32_t id) {
  absl::MutexLock lck(&_stats_m);
  if (id == -1)
    *response = _stats.sql_manager();
  else {
    *response = SqlManagerStats();
    auto c = response->add_connections();
    c->CopyFrom(_stats.sql_manager().connections(id));
  }
}

void center::get_sql_connection_size(GenericSize* response) {
  absl::MutexLock lck(&_stats_m);
  response->set_size(_stats.sql_manager().connections().size());
}

void center::get_conflict_manager_stats(ConflictManagerStats* response) {
  absl::MutexLock lck(&_stats_m);
  *response = _stats.conflict_manager();
}

int center::get_json_stats_file_creation(void) {
  return _json_stats_file_creation;
}

bool center::muxer_stats(const std::string& name, MuxerStats* response) {
  absl::MutexLock lck(&_stats_m);
  if (!_stats.processing().muxers().contains(name))
    return false;
  else {
    *response = _stats.processing().muxers().at(name);
    return true;
  }
}

MuxerStats* center::muxer_stats(const std::string& name) {
  absl::MutexLock lck(&_stats_m);
  return &(*_stats.mutable_processing()->mutable_muxers())[name];
}

void center::get_processing_stats(ProcessingStats* response) {
  absl::MutexLock lck(&_stats_m);
  *response = _stats.processing();
}

void center::clear_muxer_queue_file(const std::string& name) {
  absl::MutexLock lck(&_stats_m);
  if (_stats.processing().muxers().contains(name))
    _stats.mutable_processing()
        ->mutable_muxers()
        ->at(name)
        .mutable_queue_file()
        ->Clear();
}

void center::lock() {
  _stats_m.Lock();
}

void center::unlock() {
  _stats_m.Unlock();
}

const BrokerStats& center::stats() const {
  return _stats;
}
