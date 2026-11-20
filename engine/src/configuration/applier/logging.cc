/**
 * Copyright 2011-2014,2018-2024 Centreon
 * Copyright 2011-2014,2017-2024 Centreon
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
#include "com/centreon/engine/configuration/applier/logging.hh"
#include "com/centreon/engine/globals.hh"

using namespace com::centreon::engine::configuration;

void applier::logging::apply(State& config) {
  if (verify_config || test_scheduling)
    return;
  if (config.log_legacy_enabled())
    config_logger->warn(
        "log_legacy_enabled is deprecated and will be removed in a future "
        "version. Please disable it in your configuration.");
}

applier::logging& applier::logging::instance() {
  static applier::logging instance;
  return instance;
}

void applier::logging::clear() {}

applier::logging::logging() {}

applier::logging::logging(State& config) {
  apply(config);
}

applier::logging::~logging() noexcept {}
