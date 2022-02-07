/*
** Copyright 2020-2021 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/stats/center.hh"

#include <fmt/format.h>
#include <google/protobuf/util/json_util.h>

#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/filesystem.hh"
#include "com/centreon/broker/version.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::stats;
using namespace google::protobuf::util;
using namespace com::centreon::broker::modules;

center* center::_instance{nullptr};

center& center::instance() {
  assert(_instance);
  return *_instance;
}

void center::load() {
  if (_instance == nullptr)
    _instance = new center();
}

void center::unload() {
  delete _instance;
  _instance = nullptr;
}

center::center() : _strand(pool::instance().io_context()) {
  *_stats.mutable_version() = version::string;
  *_stats.mutable_asio_version() =
      fmt::format("{}.{}.{}", ASIO_VERSION / 100000, ASIO_VERSION / 100 % 1000,
                  ASIO_VERSION % 100);
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

  /*Start the thread pool */
  pool::instance().start_stats(_stats.mutable_pool_stats());
}

/**
 * @brief The destructor.
 */
center::~center() {
  /* Before destroying the strand, we have to wait it is really empty. We post
   * a last action and wait it is over. */
  std::promise<bool> p;
  std::future<bool> f{p.get_future()};
  _strand.post([&p] { p.set_value(true); });
  f.get();
  pool::instance().stop_stats();
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
  std::promise<EngineStats*> p;
  std::future<EngineStats*> retval = p.get_future();
  _strand.post([this, &p] {
    auto eng = _stats.mutable_processing()->mutable_engine();
    p.set_value(eng);
  });
  return retval.get();
}

SqlManagerStats* center::register_mysql_manager() {
  std::promise<SqlManagerStats*> p;
  std::future<SqlManagerStats*> retval = p.get_future();
  _strand.post([this, &p] {
    auto m = _stats.mutable_sql_manager();
    p.set_value(m);
  });
  return retval.get();
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
bool center::unregister_muxer(const std::string& name) {
  std::promise<bool> p;
  std::future<bool> retval = p.get_future();
  _strand.post([this, &p, name] {
    _stats.mutable_processing()->mutable_muxers()->erase(name);
    p.set_value(true);
  });
  return retval.get();
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
  _strand.post([this, name = std::move(name), queue = std::move(queue_file),
                size, unack] {
    auto ms = &(*_stats.mutable_processing()->mutable_muxers())[name];
    if (ms) {
      ms->mutable_queue_file()->set_name(std::move(queue));
      ms->set_total_events(size);
      ms->set_unacknowledged_events(unack);
    }
  });
}

void center::init_queue_file(std::string muxer,
                             std::string queue_file,
                             uint32_t max_file_size) {
  _strand.post([this, name = std::move(muxer), queue = std::move(queue_file),
                max_file_size] {
    auto qfs = (&(*_stats.mutable_processing()->mutable_muxers())[name])
                   ->mutable_queue_file();
    if (qfs) {
      qfs->set_name(std::move(queue));
      qfs->set_max_file_size(max_file_size);
    }
  });
}

/**
 * @brief When a feeder needs to write statistics, it primarily has to
 * call this function to be registered in the statistic center and to get
 * a pointer for its statistics. It is prohibited to directly write into this
 * pointer. We must use the center member functions for this purpose.
 *
 * @param name
 *
 * @return A pointer to the feeder statistics.
 */
// FeederStats* center::register_feeder(EndpointStats* ep_stats,
//                                     const std::string& name) {
//  std::promise<FeederStats*> p;
//  std::future<FeederStats*> retval = p.get_future();
//  _strand.post([this, ep_stats, &p, &name] {
//    auto ep = &(*ep_stats->mutable_feeder())[name];
//    p.set_value(ep);
//  });
//  return retval.get();
//}

// bool center::unregister_feeder(EndpointStats* ep_stats,
//                               const std::string& name) {
//  std::promise<bool> p;
//  std::future<bool> retval = p.get_future();
//  _strand.post([this, ep_stats, &p, &name] {
//    auto ep = (*ep_stats->mutable_feeder()).erase(name);
//    p.set_value(true);
//  });
//  return retval.get();
//}

// MysqlConnectionStats* center::register_mysql_connection(
//    MysqlManagerStats* stats) {
//  std::promise<MysqlConnectionStats*> p;
//  std::future<MysqlConnectionStats*> retval = p.get_future();
//  _strand.post([this, stats, &p] {
//    auto ep = stats->add_connections();
//    p.set_value(ep);
//  });
//  return retval.get();
//}

// bool center::unregister_mysql_connection(MysqlConnectionStats* c) {
//  std::promise<bool> p;
//  std::future<bool> retval = p.get_future();
//  _strand.post([this, c, &p] {
//    for (auto
//             it =
//                 _stats.mutable_mysql_manager()->mutable_connections()->begin(),
//             end =
//             _stats.mutable_mysql_manager()->mutable_connections()->end();
//         it != end; ++it) {
//      if (&(*it) == c) {
//        _stats.mutable_mysql_manager()->mutable_connections()->erase(it);
//        break;
//      }
//    }
//    p.set_value(true);
//  });
//  return retval.get();
//}

/**
 * @brief When an endpoint needs to write statistics, it primarily has to
 * call this function to be registered in the statistic center and to get
 * a pointer to its statistics. It is prohibited to directly write into this
 * pointer. We must use the center member function for this purpose.
 *
 * @param name
 *
 * @return A pointer to the endpoint statistics.
 */
// EndpointStats* center::register_endpoint(const std::string& name) {
//  std::promise<EndpointStats*> p;
//  std::future<EndpointStats*> retval = p.get_future();
//  _strand.post([this, &p, &name] {
//    auto ep = _stats.add_endpoint();
//    ep->set_name(name);
//    *ep->mutable_memory_file_path() = fmt::format(
//        "{}.memory.{}", config::applier::state::instance().cache_dir(), name);
//    *ep->mutable_queue_file_path() = fmt::format(
//        "{}.queue.{}", config::applier::state::instance().cache_dir(), name);
//
//    p.set_value(ep);
//  });
//  return retval.get();
//}

// bool center::unregister_endpoint(const std::string& name) {
//  std::promise<bool> p;
//  std::future<bool> retval = p.get_future();
//  _strand.post([this, &p, &name] {
//    for (auto it = _stats.mutable_endpoint()->begin();
//         it != _stats.mutable_endpoint()->end(); ++it) {
//      if (it->name() == name) {
//        _stats.mutable_endpoint()->erase(it);
//        break;
//      }
//    }
//    p.set_value(true);
//  });
//  return retval.get();
//}

/**
 * @brief To allow the conflict manager to send statistics, it has to call this
 * function to get a pointer to its statistics container.
 * It is prohibited to directly write into the returned pointer. We must use
 * the center member functions for this purpose.
 *
 * @return A pointer to the conflict_manager statistics.
 */
ConflictManagerStats* center::register_conflict_manager() {
  std::promise<ConflictManagerStats*> p;
  std::future<ConflictManagerStats*> retval = p.get_future();
  _strand.post([this, &p] {
    auto cm = _stats.mutable_conflict_manager();
    p.set_value(cm);
  });
  return retval.get();
}

/**
 * @brief To allow the conflict manager to send statistics, it has to call this
 * function to get a pointer to its statistics container.
 * It is prohibited to directly write into the returned pointer. We must use
 * the center member functions for this purpose.
 *
 * @return A pointer to the module statistics.
 */
// ModuleStats* center::register_modules() {
//  std::promise<ModuleStats*> p;
//  std::future<ModuleStats*> retval = p.get_future();
//  _strand.post([this, &p] {
//    auto m = _stats.add_modules();
//    p.set_value(m);
//  });
//  return retval.get();
//}

/**
 * @brief Convert the protobuf statistics object to a json string.
 *
 * @return a string with the object in json format.
 */
std::string center::to_string() {
  std::promise<std::string> p;
  std::future<std::string> retval = p.get_future();
  _strand.post(
      [&s = this->_stats, &p, &tmpnow = this->_json_stats_file_creation] {
        const JsonPrintOptions options;
        std::string retval;
        std::time_t now;
        time(&now);
        tmpnow = (int)now;
        s.set_now(now);
        MessageToJsonString(s, &retval, options);
        p.set_value(std::move(retval));
      });

  return retval.get();
}

void center::get_mysql_manager_stats(SqlManagerStats* response) {
  std::promise<void> p;
  std::future<void> done = p.get_future();
  _strand.post([&s = this->_stats, &p, response] {
    *response = s.sql_manager();
    p.set_value();
  });

  // We wait for the response.
  done.wait();
}

void center::get_conflict_manager_stats(ConflictManagerStats* response) {
  std::promise<bool> p;
  std::future<bool> done = p.get_future();
  _strand.post([&s = this->_stats, &p, response] {
    *response = s.conflict_manager();
    p.set_value(true);
  });

  // We wait for the response.
  done.get();
}

int center::get_json_stats_file_creation(void) {
  return _json_stats_file_creation;
}

bool center::muxer_stats(const std::string& name, MuxerStats* response) {
  std::promise<bool> p;
  std::future<bool> done = p.get_future();
  _strand.post([&s = this->_stats, &p, name, response] {
    if (!s.processing().muxers().contains(name))
      p.set_value(false);
    else {
      *response = s.processing().muxers().at(name);
      p.set_value(true);
    }
  });
  return done.get();
}

MuxerStats* center::muxer_stats(const std::string& name) {
  std::promise<MuxerStats*> p;
  _strand.post([&s = this->_stats, &p, &name] {
    MuxerStats* ms = &(*s.mutable_processing()->mutable_muxers())[name];
    p.set_value(ms);
  });
  return p.get_future().get();
}

void center::get_processing_stats(ProcessingStats* response) {
  std::promise<void> p;
  _strand.post([&s = this->_stats, &p, response] {
    *response = s.processing();
    p.set_value();
  });
  p.get_future().get();
}

void center::clear_muxer_queue_file(const std::string& name) {
  std::promise<void> p;
  _strand.post([&s = this->_stats, &p, &name] {
    if (s.processing().muxers().contains(name))
      s.mutable_processing()
          ->mutable_muxers()
          ->at(name)
          .mutable_queue_file()
          ->Clear();
    p.set_value();
  });
  p.get_future().wait();
}
