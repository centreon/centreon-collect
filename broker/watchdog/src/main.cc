/**
 * Copyright 2015,2017, 2026 Centreon
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

#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <algorithm>

#include <csignal>
#include <cstring>

#include "com/centreon/broker/watchdog/configuration_parser.hh"
#include "com/centreon/broker/watchdog/instance.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::watchdog;

static char const* config_filename = nullptr;
static bool should_exit{false};
std::unique_ptr<spdlog::logger> logger;
static configuration config;
static std::unordered_map<std::string, instance*> instances;
static bool sighup{false};

/**
 *  Print the help.
 */
static void print_help() {
  std::cout << "USAGE: cbwd configuration_file" << std::endl;
}

/**
 *  This function applies a new configuration in replacement of the current one.
 *
 * @param cfg The new configuration to set.
 */
static void apply_new_configuration(configuration const& cfg) {
  /* The logger is only rebuilt when the log file changes, otherwise the
   * current one, that already writes into this file, is kept. */
  if (config.get_log_filename() != cfg.get_log_filename()) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::warn);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        cfg.get_log_filename(), false);
    file_sink->set_level(spdlog::level::trace);

    logger = std::make_unique<spdlog::logger>(
        "cbwd", spdlog::sinks_init_list{console_sink, file_sink});
    logger->flush_on(spdlog::level::trace);
    logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [cbwd] [%l] %v");
  }

  std::set<std::string> to_update;
  std::set<std::string> to_delete;
  std::set<std::string> to_create;

  // Old configs that aren't present in the new should be deleted.
  // Old configs that are present in the new should be updated
  // or deleted/recreated.
  for (auto it = config.get_instances_configuration().begin(),
            end = config.get_instances_configuration().end();
       it != end; ++it) {
    instance_configuration new_config(
        cfg.get_instance_configuration(it->first));
    if (new_config.is_empty())
      to_delete.insert(it->first);
    else if (!it->second.same_child(new_config)) {
      to_delete.insert(it->first);
      to_create.insert(it->first);
    } else
      to_update.insert(it->first);
  }

  // Delete old processes.
  for (auto it = to_delete.begin(), end = to_delete.end(); it != end; ++it) {
    auto found = instances.find(*it);
    if (found != instances.end()) {
      instance* to_remove{found->second};
      instances.erase(found);
      delete to_remove;
    }
  }

  // New configs that aren't present in the old should be created.
  for (configuration::instance_map::const_iterator
           it(cfg.get_instances_configuration().begin()),
       end(cfg.get_instances_configuration().end());
       it != end; ++it)
    if (!config.instance_exists(it->first))
      to_create.insert(it->first);

  // Update processes.
  for (std::set<std::string>::const_iterator it(to_update.begin()),
       end(to_update.end());
       it != end; ++it) {
    std::unordered_map<std::string, instance*>::iterator found(
        instances.find(*it));
    if (found != instances.end()) {
      found->second->merge_configuration(cfg.get_instance_configuration(*it));
      found->second->update();
    }
  }

  // Start new processes.
  for (std::set<std::string>::const_iterator it(to_create.begin()),
       end(to_create.end());
       it != end; ++it) {
    std::unique_ptr<instance> ins(
        new instance(cfg.get_instance_configuration(*it)));
    instances.insert({*it, ins.release()});
  }

  // Save the new configuration.
  config = cfg;
}

/**
 * @brief Block the signals the watchdog waits for. They are not handled by
 * signal handlers but received synchronously by sigtimedwait() in the main
 * loop, so the death of a cbd, a reload or a stop request are processed at
 * once. This must be done before the first cbd is started (the processes
 * created by the watchdog empty their mask before execve()).
 *
 * @return The set of the blocked signals.
 */
static sigset_t block_signals() {
  sigset_t retval;
  sigemptyset(&retval);
  sigaddset(&retval, SIGCHLD);
  sigaddset(&retval, SIGHUP);
  sigaddset(&retval, SIGINT);
  sigaddset(&retval, SIGTERM);
  if (sigprocmask(SIG_BLOCK, &retval, nullptr) < 0)
    throw msg_fmt("can't block the signals: {}", strerror(errno));
  return retval;
}

/**
 * @brief Reap all the stopped cbd and restart them (or schedule their
 * restart).
 */
static void reap_stopped_instances() {
  int status;
  pid_t stopped_pid;
  while ((stopped_pid = waitpid(0, &status, WNOHANG)) > 0) {
    logger->error("cbd instance with PID {} has stopped, attempt to restart it",
                  stopped_pid);
    for (auto& [name, inst] : instances) {
      if (inst->get_pid() == stopped_pid)
        inst->on_stopped();
    }
  }
}

/**
 * @brief Restart the instances whose delayed restart is due, and compute how
 * long the main loop can wait before the next one.
 *
 * @return The wait duration before the next restart (max_wait if none).
 */
static std::chrono::steady_clock::duration restart_due_instances() {
  /* The longest wait of the main loop. It is only a safety net, the death of
   * a cbd is notified by SIGCHLD. */
  static constexpr std::chrono::seconds max_wait{5};
  auto now = std::chrono::steady_clock::now();
  std::chrono::steady_clock::duration retval = max_wait;
  for (auto& [name, inst] : instances) {
    inst->restart_if_due(now);
    if (auto at = inst->restart_at())
      retval = std::min(retval, *at - now);
  }
  return retval;
}

/**
 *  Centreon Watchdog entry point.
 *
 *  @param[in] argc  Parameter count.
 *  @param[in] argv  Parameter values.
 *
 *  @return  0 on success.
 */
int main(int argc, char** argv) {
  // Check arguments.

  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::warn);
  logger = std::make_unique<spdlog::logger>("cbwd", console_sink);
  logger->flush_on(spdlog::level::trace);
  logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [cbwd] [%l] %v");

  if (argc != 2 || ::strcmp(argv[1], "-h") == 0) {
    print_help();
    return 0;
  }

  config_filename = argv[1];

  configuration config;

  try {
    configuration_parser parser;
    config = parser.parse(config_filename);
  } catch (std::exception const& e) {
    logger->error("Could not parse the configuration file '{}': {}",
                  config_filename, e.what());
    return 2;
  }

  int retval = 0;

  // Create the main event loop.
  try {
    sigset_t signals = block_signals();
    apply_new_configuration(config);
    while (!should_exit) {
      reap_stopped_instances();

      if (sighup) {
        // Sighup : should reload...
        sighup = false;
        configuration config;
        try {
          configuration_parser parser;
          config = parser.parse(config_filename);
          apply_new_configuration(config);
        } catch (std::exception const& e) {
          logger->error("Could not parse the new configuration: {}", e.what());
        }
      }

      /* We wait for a signal, or for the next delayed restart. */
      auto wait_duration = restart_due_instances();
      auto secs =
          std::chrono::duration_cast<std::chrono::seconds>(wait_duration);
      struct timespec ts;
      ts.tv_sec = std::max<time_t>(secs.count(), 0);
      ts.tv_nsec =
          std::max<long>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                             wait_duration - secs)
                             .count(),
                         0);
      switch (sigtimedwait(&signals, nullptr, &ts)) {
        case SIGTERM:
        case SIGINT:
          should_exit = true;
          break;
        case SIGHUP:
          sighup = true;
          break;
        default:
          /* SIGCHLD or timeout: stopped instances are reaped at the next
           * iteration. */
          break;
      }
    }
  } catch (std::exception const& e) {
    std::cerr << "watchdog: " << e.what() << std::endl;
    retval = 1;
  }

  for (auto it = instances.begin(), end = instances.end(); it != end; ++it)
    it->second->stop();

  return retval;
}
