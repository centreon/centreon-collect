/**
 * Copyright 2024 Centreon
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

#include "com/centreon/broker/unified_sql/engine_configurator.hh"

using namespace com::centreon::broker::unified_sql;

engine_configurator::engine_configurator(
    uint32_t instance_id,
    const std::shared_ptr<spdlog::logger>& logger)
    : _instance_id{instance_id}, _logger{logger} {}

void engine_configurator::apply() {}
