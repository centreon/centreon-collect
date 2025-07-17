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

#include "agent_info.hh"
#include "version.hh"

static std::string _os;
static std::string _os_version;
static std::string _encryption_test;

/**
 * @brief set _encryption_test in order to enable engine to test encryption
 *
 * @param encryption_test
 */
void com::centreon::agent::set_encryption_test(std::string&& encryption_test) {
  _encryption_test = std::move(encryption_test);
}

/**
 * @brief read os version
 * to call at the beginning of program
 *
 */
void com::centreon::agent::read_os_version() {
  std::fstream os_release("/etc/os-release", std::fstream::in);
  if (os_release.is_open()) {
    enum { os_found = 1, version_found = 2, all_found = 3 };
    unsigned found = 0;
    std::string line;
    while (std::getline(os_release, line) && found != all_found) {
      if (!line.compare(0, 3, "ID=")) {
        line.erase(0, 3);
        boost::algorithm::trim_if(line, [](const char c) {
          return c == '"' || c == ' ' || c == '\'';
        });
        _os = line;
        found |= os_found;
      } else if (!line.compare(0, 11, "VERSION_ID=")) {
        line.erase(0, 11);
        boost::algorithm::trim_if(line, [](const char c) {
          return c == '"' || c == ' ' || c == '\'';
        });
        _os_version = line;
        found |= version_found;
      }
    }
  }
}

/**
 * @brief fill agent_info with agent and os versions
 *
 * @param supervised_host host configured
 * @param agent_info pointer to object to fill
 */
void com::centreon::agent::fill_agent_info(
    const std::string& supervised_host,
    ::com::centreon::agent::AgentInfo* agent_info) {
  agent_info->mutable_centreon_version()->set_major(
      CENTREON_AGENT_VERSION_MAJOR);
  agent_info->mutable_centreon_version()->set_minor(
      CENTREON_AGENT_VERSION_MINOR);
  agent_info->mutable_centreon_version()->set_patch(
      CENTREON_AGENT_VERSION_PATCH);
  agent_info->set_host(supervised_host);
  agent_info->set_os(_os);
  agent_info->set_os_version(_os_version);
  if (!_encryption_test.empty()) {
    agent_info->set_encryption_test(_encryption_test);
  }
}
