/**
 * Copyright 2020-2023 Centreon
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

#ifndef CENTREON_BROKER_CORE_INC_COM_CENTREON_STATS_HELPER_HH_
#define CENTREON_BROKER_CORE_INC_COM_CENTREON_STATS_HELPER_HH_

#include <nlohmann/json.hpp>

namespace com::centreon::broker::stats {

void get_generic_stats(nlohmann::json& object) noexcept;
void get_mysql_stats(nlohmann::json& object) noexcept;
void get_loaded_module_stats(std::vector<nlohmann::json>& object) noexcept;
bool get_endpoint_stats(std::vector<nlohmann::json>& object);

}  // namespace com::centreon::broker::stats

#endif  // CENTREON_BROKER_CORE_INC_COM_CENTREON_STATS_HELPER_HH_
