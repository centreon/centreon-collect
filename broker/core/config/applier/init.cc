/**
 * Copyright 2011-2013, 2021-2024 Centreon
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

#include <condition_variable>
#include <deque>
#include <future>
#include <list>
#include <stack>
#include <thread>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include <boost/asio.hpp>

namespace asio = boost::asio;

#include <spdlog/fmt/ostr.h>

#include "broker/core/config/applier/init.hh"

#include "broker/core/config/applier/endpoint.hh"
#include "com/centreon/broker/file/disk_accessor.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/sql/mysql_manager.hh"
#include "com/centreon/broker/time/timezone_manager.hh"
#include "com/centreon/common/pool.hh"

using com::centreon::common::log_v2::log_v2;

namespace com::centreon::broker::config::applier {

std::atomic<applier_state> mode{not_started};

/**
 * @brief Load necessary structures. It initializes exactly the same structures
 * as init(const config::state& conf) just with detailed parameters.
 *
 * @param peer_type The type of peer this broker is.
 * @param engine_conf_version The version of the engine configuration or "" if
 * not applicable.
 * @param n_thread number of threads in the pool.
 * @param name The broker name to give to this cbd instance.
 * @param event_queues_total_size The total size in bytes of the event queues.
 * It is used to avoid a full disk when writing events to disk.
 */
template <typename State>
void init(const std::string& engine_conf_version,
          size_t n_thread,
          const std::string& /* name */,
          size_t event_queues_total_size) {
  /* Load singletons.
   * Why so many?
   * The stats::center is now embedded by each user. We could avoid the
   * singleton but as the pool is going to move to common, I don't have a view
   * on the impact of this change, so I prefer to keep it as a singleton but
   * starting the job to embed the center.
   * For the multipliexing::engine, we have a similar issue. Muxers embed the
   * engine, so we could avoid the singleton, but it is possible to access the
   * engine from stream thanks to the singleton. As this functionality is still
   * used, we must keep the singleton.
   */
  com::centreon::common::pool::set_pool_size(n_thread);
  state::load<State>(engine_conf_version);
  mysql_manager::load();
  file::disk_accessor::load(event_queues_total_size);
  io::protocols::load();
  io::events::load();
  multiplexing::engine::load();
  endpoint::load();
  mode = initialized;
}

/**
 *  Unload necessary structures.
 */
void deinit() {
  mode = finished;
  auto logger = log_v2::instance().get(log_v2::CORE);
  logger->info("unloading applier::endpoint");
  endpoint::unload();
  {
    auto eng = multiplexing::engine::instance_ptr();
    if (eng) {
      multiplexing::engine::unload();
    }
  }
  state::unload();
  io::events::unload();
  io::protocols::unload();
  mysql_manager::unload();
  file::disk_accessor::unload();
}

// Explicit instanciations of templates
#if defined BROKER_COMPILATION

template void init<broker_state>(const std::string&,
                                 size_t,
                                 const std::string&,
                                 size_t);
#elif defined CBMOD_COMPILATION

template void init<cbmod_state>(const std::string&,
                                size_t,
                                const std::string&,
                                size_t);
#endif

}  // namespace com::centreon::broker::config::applier
