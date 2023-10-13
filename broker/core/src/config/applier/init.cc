/**
 * Copyright 2011-2013, 2021-2023 Centreon
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

#include <absl/container/flat_hash_set.h>

#include <boost/asio.hpp>
#include <condition_variable>
#include <deque>
#include <future>
#include <list>
#include <stack>
#include <thread>
#include <vector>

namespace asio = boost::asio;

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include "com/centreon/broker/config/applier/endpoint.hh"
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/broker/sql/mysql_manager.hh"
#include "com/centreon/broker/time/timezone_manager.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;

std::atomic<config::applier::applier_state> config::applier::mode{not_started};

extern std::shared_ptr<asio::io_context> g_io_context;
extern bool g_io_context_started;

/**
 * @brief Load necessary structures. It initializes exactly the same structures
 * as init(const config::state& conf) just with detailed parameters.
 *
 * @param n_thread number of threads in the pool.
 * @param name The broker name to give to this cbd instance.
 */
void config::applier::init(size_t n_thread,
                           const std::string&,
                           size_t event_queues_total_size) {
  // Load singletons.
  pool::load(g_io_context, n_thread);
  g_io_context_started = true;
  stats::center::load();
  mysql_manager::load();
  config::applier::state::load();
  file::disk_accessor::load(event_queues_total_size);
  io::protocols::load();
  io::events::load();
  multiplexing::engine::load();
  config::applier::endpoint::load();
  mode = initialized;
}

/**
 *  Unload necessary structures.
 */
void config::applier::deinit() {
  mode = finished;
  auto logger = log_v2::instance().get(log_v2::CORE);
  logger->info("unloading applier::endpoint");
  config::applier::endpoint::unload();
  {
    std::weak_ptr<multiplexing::engine> eng =
        multiplexing::engine::instance_ptr();
    if (!eng.expired()) {
      logger->info("unloading multiplexing::engine");
      multiplexing::engine::unload();
    }
    assert(eng.expired());
  }
  logger->info("unloading state");
  config::applier::state::unload();
  logger->info("unloading io::events");
  io::events::unload();
  logger->info("unloading io::protocols");
  io::protocols::unload();
  logger->info("unloading io::mysql_manager");
  mysql_manager::unload();
  logger->info("unloading stats::center");
  stats::center::unload();
  logger->info("unloading file::disk_accessor");
  file::disk_accessor::unload();

  pool::unload();
}

/**
 * @brief Load necessary structures.
 *
 * @param conf The configuration used to initialize the all.
 */
void config::applier::init(const config::state& conf) {
  log_v2::instance().apply(conf.log_conf());
  init(conf.pool_size(), conf.broker_name(), conf.event_queues_total_size());
}
