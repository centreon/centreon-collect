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

#include <iphlpapi.h>

#include "agent_info.hh"
#include "ntdll.hh"
#include "version.hh"
#include "windows_util.hh"

// Link with Iphlpapi.lib
#pragma comment(lib, "iphlpapi.lib")

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
    const std::string& host_template,
    ::com::centreon::agent::AgentInfo* agent_info,
    const std::shared_ptr<spdlog::logger>& logger) {
  agent_info->mutable_centreon_version()->set_major(
      CENTREON_AGENT_VERSION_MAJOR);
  agent_info->mutable_centreon_version()->set_minor(
      CENTREON_AGENT_VERSION_MINOR);
  agent_info->mutable_centreon_version()->set_patch(
      CENTREON_AGENT_VERSION_PATCH);
  agent_info->set_host(supervised_host);
  agent_info->set_host_template(host_template);
  agent_info->set_os(_os);
  agent_info->set_os_version(_os_version);
  agent_info->set_encryption_ready(true);

  PIP_ADAPTER_ADDRESSES adapters = nullptr;

  constexpr ULONG get_adapters_adresses_flags =
      GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
      GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;
  // get needed size
  ULONG out_buff_len = 0;
  DWORD res = GetAdaptersAddresses(AF_UNSPEC, get_adapters_adresses_flags,
                                   nullptr, nullptr, &out_buff_len);
  if (res == ERROR_BUFFER_OVERFLOW) {
    adapters = (PIP_ADAPTER_ADDRESSES)malloc(out_buff_len);
    res = GetAdaptersAddresses(AF_UNSPEC, get_adapters_adresses_flags, nullptr,
                               adapters, &out_buff_len);
    if (res == ERROR_SUCCESS) {
      for (PIP_ADAPTER_ADDRESSES adapter = adapters; adapter;
           adapter = adapter->Next) {
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
          continue;
        }
        for (IP_ADAPTER_UNICAST_ADDRESS* addr = adapter->FirstUnicastAddress;
             addr; addr = addr->Next) {
          char ip_str[INET6_ADDRSTRLEN];

          sockaddr* sa = addr->Address.lpSockaddr;

          if (sa->sa_family == AF_INET) {
            sockaddr_in* ipv4 = (sockaddr_in*)sa;
            inet_ntop(AF_INET, &ipv4->sin_addr, ip_str, sizeof(ip_str));
            agent_info->add_ips(ip_str);
          } else if (sa->sa_family == AF_INET6) {
            sockaddr_in6* ipv6 = (sockaddr_in6*)sa;
            inet_ntop(AF_INET6, &ipv6->sin6_addr, ip_str, sizeof(ip_str));
            agent_info->add_ips(ip_str);
          }
        }
      }
    } else {
      SPDLOG_LOGGER_ERROR(logger, "fail to get interface addresses: {}",
                          error_as_string(res));
    }
    free(adapters);
  } else {
    SPDLOG_LOGGER_ERROR(logger, "fail to get interface addresses: {}",
                        error_as_string(res));
  }
}
