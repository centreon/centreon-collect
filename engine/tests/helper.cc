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

extern configuration::indexed_state pb_indexed_config;

void init_config_state() {
  /* Cleanup */
  pb_indexed_config.state().Clear();
  if (!cbm)
    cbm = std::make_unique<com::centreon::broker::neb::cbmod_test>("");

  configuration::state_helper cfg_hlp(&pb_indexed_config.state());
  pb_indexed_config.state().set_log_file_line(true);
  pb_indexed_config.state().set_log_file("");

  log_v2_config log_conf("engine-tests",
                         log_v2_config::logger_type::LOGGER_STDOUT,
                         pb_indexed_config.state().log_flush_period(),
                         pb_indexed_config.state().log_pid(),
                         pb_indexed_config.state().log_file_line());

  log_conf.set_level("checks", "debug");
  log_conf.set_level("events", "trace");
  log_conf.set_level("notifications", "trace");

  log_v2::instance().apply(log_conf);

  // Hack to instanciate the logger.
  configuration::applier::logging::instance().apply(pb_indexed_config.state());

  checks::checker::init(true);
}

void deinit_config_state(void) {
  pb_indexed_config.state().Clear();

  configuration::applier::state::instance().clear();
  checks::checker::deinit();
}
