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

#include "com/centreon/engine/commands/otel_interface.hh"

using namespace com::centreon::engine::commands::otel;

/**
 * @brief singleton used to make the bridge between engine and otel module
 *
 */
std::shared_ptr<com::centreon::engine::commands::otel::open_telemetry_base>
    com::centreon::engine::commands::otel::open_telemetry_base::_instance;

void host_serv_list::register_host_serv(
    const std::string& host,
    const std::string& service_description) {
  absl::WriterMutexLock l(&_data_m);
  _data[host].insert(service_description);
}

void host_serv_list::remove(const std::string& host,
                            const std::string& service_description) {
  absl::WriterMutexLock l(&_data_m);
  auto host_search = _data.find(host);
  if (host_search != _data.end()) {
    host_search->second.erase(service_description);
    if (host_search->second.empty()) {
      _data.erase(host_search);
    }
  }
}

/**
 * @brief test if a host serv pair is contained in list
 *
 * @param host
 * @param service_description
 * @return true found
 * @return false  not found
 */
bool host_serv_list::contains(const std::string& host,
                              const std::string& service_description) const {
  absl::ReaderMutexLock l(&_data_m);
  auto host_search = _data.find(host);
  if (host_search != _data.end()) {
    return host_search->second.contains(service_description);
  }
  return false;
}
