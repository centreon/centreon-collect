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
#include "ntdll.hh"
#include "version.hh"

static std::string _os;
static std::string _os_version;

/**
 * @brief read os version
 * to call at the beginning of program
 *
 */
void com::centreon::agent::read_os_version() {
  RTL_OSVERSIONINFOEXW osvi;
  ZeroMemory(&osvi, sizeof(osvi));
  osvi.dwOSVersionInfoSize = sizeof(osvi);
  if (rtl_get_version(&osvi) == 0) {
    _os = osvi.wProductType == VER_NT_SERVER ? "windows-server" : "windows";
    _os_version = std::to_string(osvi.dwMajorVersion) + "." +
                  std::to_string(osvi.dwMinorVersion) + "." +
                  std::to_string(osvi.dwBuildNumber);
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
  agent_info->set_encryption_ready(true);
}