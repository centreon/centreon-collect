/**
 * Copyright 2019 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "helper.hh"

#include "cbmod_test.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/configuration/applier/logging.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"

#include "common/log_v2/config.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::engine;
using com::centreon::common::log_v2::log_v2;
using log_v2_config = com::centreon::common::log_v2::config;

#ifdef LEGACY_CONF
extern configuration::state* config;
#else
extern configuration::State pb_config;
#endif

#ifdef LEGACY_CONF
void init_config_state() {
  if (config == nullptr)
    config = new configuration::state;

  config->log_file_line(true);
  config->log_file("");

  log_v2_config log_conf(
      "engine-tests", log_v2_config::logger_type::LOGGER_STDOUT,
      config->log_flush_period(), config->log_pid(), config->log_file_line());

  log_v2::instance().apply(log_conf);

  // Hack to instanciate the logger.
  configuration::applier::logging::instance().apply(*config);

  checks::checker::init(true);
}
#else
void init_config_state() {
  /* Cleanup */
  pb_config.Clear();
  if (!cbm)
    cbm = std::make_unique<com::centreon::broker::neb::cbmod_test>();

  configuration::state_helper cfg_hlp(&pb_config);
  pb_config.set_log_file_line(true);
  pb_config.set_log_file("");

  log_v2_config log_conf("engine-tests",
                         log_v2_config::logger_type::LOGGER_STDOUT,
                         pb_config.log_flush_period(), pb_config.log_pid(),
                         pb_config.log_file_line());

  log_v2::instance().apply(log_conf);

  // Hack to instanciate the logger.
  configuration::applier::logging::instance().apply(pb_config);

  checks::checker::init(true);
}
#endif

void deinit_config_state(void) {
#ifdef LEGACY_CONF
  delete config;
  config = nullptr;
#else
  pb_config.Clear();
#endif

  configuration::applier::state::instance().clear();
  checks::checker::deinit();
}
