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

#include <ifaddrs.h>
#include <netdb.h>
#include <sys/utsname.h>

#include <fstream>
#include <set>

#include <absl/strings/match.h>

#include "agent_info.hh"
#include "version.hh"

static std::string _os;
static std::string _os_name;
static std::string _os_version;
static std::string _arch;
static std::string _machine_id;

static void _trim_os_release_value(std::string& value) {
  boost::algorithm::trim_if(
      value, [](const char c) { return c == '"' || c == ' ' || c == '\''; });
}

/**
 * @brief convert a uname machine to an OpenTelemetry host.arch value
 *
 * @param machine uname -m value such as x86_64
 * @return the OTel well-known value, or machine itself if there is none
 */
std::string com::centreon::agent::otel_arch(std::string_view machine) {
  if (machine == "x86_64" || machine == "amd64")
    return "amd64";
  if (machine == "aarch64" || machine == "arm64")
    return "arm64";
  if (absl::StartsWith(machine, "arm"))
    return "arm32";
  if (machine == "i386" || machine == "i486" || machine == "i586" ||
      machine == "i686")
    return "x86";
  if (absl::StartsWith(machine, "ppc64"))
    return "ppc64";
  if (machine == "ppc")
    return "ppc32";
  // s390x and ia64 are already OTel values
  return std::string(machine);
}

/**
 * @brief read a machine-id file content
 *
 * @param path /etc/machine-id or /var/lib/dbus/machine-id
 * @return the id, or an empty string if the file is absent or its content is
 * not 32 hexadecimal characters (systemd leaves "uninitialized" on first boot)
 */
std::string com::centreon::agent::read_machine_id(const std::string& path) {
  std::ifstream f(path);
  std::string id;
  if (!f.is_open() || !std::getline(f, id))
    return {};
  boost::algorithm::trim(id);
  if (id.size() != 32 ||
      !std::all_of(id.begin(), id.end(),
                   [](unsigned char c) { return std::isxdigit(c); }))
    return {};
  return id;
}

/**
 * @brief read os version, architecture and machine id
 * to call at the beginning of program
 *
 */
void com::centreon::agent::read_os_version() {
  std::fstream os_release("/etc/os-release", std::fstream::in);
  if (os_release.is_open()) {
    enum { os_found = 1, version_found = 2, name_found = 4, all_found = 7 };
    unsigned found = 0;
    std::string line;
    while (std::getline(os_release, line) && found != all_found) {
      if (!line.compare(0, 3, "ID=")) {
        line.erase(0, 3);
        _trim_os_release_value(line);
        _os = line;
        found |= os_found;
      } else if (!line.compare(0, 11, "VERSION_ID=")) {
        line.erase(0, 11);
        _trim_os_release_value(line);
        _os_version = line;
        found |= version_found;
      } else if (!line.compare(0, 5, "NAME=")) {
        line.erase(0, 5);
        _trim_os_release_value(line);
        _os_name = line;
        found |= name_found;
      }
    }
  }

  struct utsname uts;
  if (!uname(&uts)) {
    _arch = otel_arch(uts.machine);
  }

  _machine_id = read_machine_id("/etc/machine-id");
  if (_machine_id.empty()) {
    _machine_id = read_machine_id("/var/lib/dbus/machine-id");
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
  agent_info->set_os_type("linux");
  agent_info->set_os_name(_os_name);
  agent_info->set_arch(_arch);
  agent_info->set_machine_id(_machine_id);
  agent_info->set_encryption_ready(true);

  struct ifaddrs* ifaddr = nullptr;
  if (getifaddrs(&ifaddr)) {
    SPDLOG_LOGGER_ERROR(logger, "fail to list interface adresses");
    return;
  }
  std::set<std::string> ips;
  for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr || !(ifa->ifa_flags & IFF_UP)) {
      continue;
    }
    if (ifa->ifa_flags & IFF_LOOPBACK) {
      continue;
    }
    int family = ifa->ifa_addr->sa_family;
    if (family == AF_INET || family == AF_INET6) {
      char host[NI_MAXHOST];
      int res = getnameinfo(ifa->ifa_addr,
                            (family == AF_INET) ? sizeof(struct sockaddr_in)
                                                : sizeof(struct sockaddr_in6),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
      if (!res) {
        /* link-local ipv6 are given with the interface zone (fe80::1%eth0),
         * meaningless out of this host */
        std::string_view ip(host);
        ips.emplace(ip.substr(0, ip.find('%')));
      } else {
        SPDLOG_LOGGER_ERROR(
            logger, "fail to get ip string from {} family address", family);
      }
    }
  }
  freeifaddrs(ifaddr);
  /* sorted and deduplicated so that two collects of an unchanged host are
   * equal and the exported host.ip attribute is stable */
  for (const std::string& ip : ips) {
    agent_info->add_ips(ip);
  }
}
