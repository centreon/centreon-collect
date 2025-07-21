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

#ifndef CENTREON_AGENT_AGENT_INFO_HH
#define CENTREON_AGENT_AGENT_INFO_HH

#include "agent.pb.h"

namespace com::centreon::agent {

void read_os_version();

void fill_agent_info(const std::string& supervised_host,
                     ::com::centreon::agent::AgentInfo* agent_info);
}  // namespace com::centreon::agent
#endif