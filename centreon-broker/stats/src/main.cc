/*
** Copyright 2012-2013 Centreon
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

#include <cerrno>
#include <cstring>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/stats/config.hh"
#include "com/centreon/broker/stats/generator.hh"
#include "com/centreon/broker/stats/parser.hh"
#include "com/centreon/broker/stats/worker.hh"

using namespace com::centreon::broker;

// Load count.
static unsigned int instances(0);

// Worker.
static std::auto_ptr<stats::generator> worker_dumper;
static std::auto_ptr<stats::worker> worker_fifo;

/**
 *  Unload current statistics workers.
 */
static void unload_workers() {
  if (worker_fifo.get()) {
    // Terminate thread.
    worker_fifo->exit();
    worker_fifo->wait();
    worker_fifo.reset();
  }
  if (worker_dumper.get()) {
    // Terminate thread.
    worker_dumper->exit();
    worker_dumper->wait();
    worker_dumper.reset();
  }
  return ;
}

extern "C" {
  /**
   *  Module version symbol. Used to check for version mismatch.
   */
  char const* broker_module_version = CENTREON_BROKER_VERSION;

  /**
   *  Module deinitialization routine.
   */
  void broker_module_deinit() {
    // Decrement instance number.
    if (!--instances)
      unload_workers();
    return ;
  }

  /**
   *  Module initialization routine.
   *
   *  @param[in] arg Configuration argument.
   */
  void broker_module_init(void const* arg) {
    // Increment instance number.
    if (!instances++) {
      // Stats module.
      logging::info(logging::high)
        << "stats: module for Centreon Broker "
        << CENTREON_BROKER_VERSION;

      // Check that stats are enabled.
      config::state const& base_cfg(*static_cast<config::state const*>(arg));
      bool loaded(false);
      std::map<std::string, std::string>::const_iterator
        it(base_cfg.params().find("stats"));
      if (it != base_cfg.params().end()) {
        try {
          // Parse configuration.
          stats::config stats_cfg;
          {
            stats::parser p;
            p.parse(stats_cfg, it->second);
          }

          // Load stats engine.
          if (!stats_cfg.get_dumper_tag().empty()
              && !stats_cfg.metrics().empty()) {
            // Create thread.
            worker_dumper.reset(new stats::generator);
            worker_dumper->run(stats_cfg, base_cfg.poller_id());
          }

          // File configured, load stats engine.
          if (!stats_cfg.get_fifo().empty()) {
            // Does file exist and is a FIFO ?
            struct stat s;
            // Stat failed, probably because of inexistant file.
            if (stat(stats_cfg.get_fifo().c_str(), &s) != 0) {
              char const* msg(strerror(errno));
              logging::config(logging::medium) << "stats: cannot stat() '"
                << stats_cfg.get_fifo() << "': " << msg;

              // Create FIFO.
              if (mkfifo(
                    stats_cfg.get_fifo().c_str(),
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
                  != 0) {
                char const* msg(strerror(errno));
                throw (exceptions::msg() << "cannot create FIFO '"
                       << stats_cfg.get_fifo() << "': " << msg);
              }
            }
            else if (!S_ISFIFO(s.st_mode))
              throw (exceptions::msg() <<  "file '" << stats_cfg.get_fifo()
                     << "' exists but is not a FIFO");

            // Create thread.
            worker_fifo.reset(new stats::worker);
            worker_fifo->run(stats_cfg.get_fifo().c_str());
          }
          loaded = true;
        }
        catch (std::exception const& e) {
          logging::config(logging::high) << "stats: "
            "engine loading failure: " << e.what();
        }
        catch (...) {
          logging::config(logging::high) << "stats: "
            "engine loading failure";
        }
      }
      if (!loaded) {
        unload_workers();
        logging::config(logging::high) << "stats: invalid stats "
          "configuration, stats engine is NOT loaded";
      }
    }
    return ;
  }
}
