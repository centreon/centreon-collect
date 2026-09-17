/**
 * Copyright 2014, 2021 Centreon
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

#include "com/centreon/broker/bam/hst_svc_mapping.hh"

using namespace com::centreon::broker::bam;
using com::centreon::common::log_v2::log_v2;

/**
 *  Get host ID by its name.
 *
 *  @param[in] hst  Host name.
 *
 *  @return Host ID, 0 if it was not found.
 */
uint64_t hst_svc_mapping::get_host_id(std::string const& hst) const {
  return get_service_id(hst, "").first;
}

/**
 *  Get service ID by its name.
 *
 *  @param[in] hst  Host name.
 *  @param[in] svc  Service description.
 *
 *  @return Pair of integers with host ID and service ID, (0, 0) if it
 *          was not found.
 */
std::pair<uint64_t, uint64_t> hst_svc_mapping::get_service_id(
    std::string const& hst,
    std::string const& svc) const {
  auto it{_mapping.find(std::make_pair(hst, svc))};
  if (it == _mapping.end()) {
    auto logger = log_v2::instance().get(log_v2::BAM);
    logger->debug(
        "hst_svc_mapping: service id for host: {} ; service: {} not found", hst,
        svc);
  }
  return it != _mapping.end() ? it->second : std::make_pair(0u, 0u);
}

/**
 *  Set the ID of a host.
 *
 *  @param[in] hst      Host name.
 *  @param[in] host_id  Host ID.
 */
void hst_svc_mapping::set_host(std::string const& hst, uint64_t host_id) {
  set_service(hst, "", host_id, 0u, true);
}

/**
 *  Set the ID of a service.
 *
 *  @param[in] hst         Host name.
 *  @param[in] svc         Service description.
 *  @param[in] host_id     Host ID.
 *  @param[in] service_id  Service ID.
 */
void hst_svc_mapping::set_service(std::string const& hst,
                                  std::string const& svc,
                                  uint64_t host_id,
                                  uint64_t service_id,
                                  bool activated) {
  _mapping[std::make_pair(hst, svc)] = std::make_pair(host_id, service_id);
  _activated_mapping[std::make_pair(host_id, service_id)] = activated;
}

/**
 *  Get if the service is activated.
 *
 *  @param[in] hst_id       The host id.
 *  @param[in] service_id   The service id.
 *
 *  @return                 True if activated.
 */
bool hst_svc_mapping::get_activated(uint64_t hst_id,
                                    uint64_t service_id) const {
  auto it{_activated_mapping.find(std::make_pair(hst_id, service_id))};
  return it == _activated_mapping.end() ? true : it->second;
}

/**
 * @brief Record whether a service is activated, without naming it.
 *
 * The name-to-id mapping and the activation flag answer two unrelated
 * questions: the first one resolves what a boolean expression names, the second
 * one tells whether a KPI points at a service the user has disabled. A KPI
 * already carries the ids of its service, so nothing has to be resolved for it
 * and the name would only be read back to be thrown away.
 *
 * @param[in] host_id    The host id.
 * @param[in] service_id The service id.
 * @param[in] activated  True if the service is activated.
 */
void hst_svc_mapping::set_activated(uint64_t host_id,
                                    uint64_t service_id,
                                    bool activated) {
  _activated_mapping[std::make_pair(host_id, service_id)] = activated;
}
