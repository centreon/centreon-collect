/**
 * Copyright 2014, 2021-2026 Centreon
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

#include "broker/core/config/applier/state.hh"

using com::centreon::common::log_v2::log_v2;

namespace com::centreon::broker::bam {

/**
 *  Get host ID by its name.
 *
 *  @param[in] hst  Host name.
 *
 *  @return Host ID, 0 if it was not found.
 */
uint64_t local_hst_svc_mapping::get_host_id(const std::string& hst) const {
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
std::pair<uint64_t, uint64_t> local_hst_svc_mapping::get_service_id(
    const std::string& hst,
    const std::string& svc) const {
  auto it{_mapping.find(std::make_pair(hst, svc))};
  if (it == _mapping.end()) {
    auto logger = log_v2::instance().get(log_v2::BAM);
    logger->debug(
        "hst_svc_mapping: service id for host: {} ; service: {} not found", hst,
        svc);
  }
  return it != _mapping.end() ? it->second : std::make_pair(0UL, 0UL);
}

/**
 *  Set the ID of a host.
 *
 *  @param[in] hst      Host name.
 *  @param[in] host_id  Host ID.
 */
void local_hst_svc_mapping::set_host(const std::string& hst, uint64_t host_id) {
  set_service(hst, "", host_id, 0UL, true);
}

/**
 *  Set the ID of a service.
 *
 *  @param[in] hst         Host name.
 *  @param[in] svc         Service description.
 *  @param[in] host_id     Host ID.
 *  @param[in] service_id  Service ID.
 *  @param[in] activated   True if the service is activated.
 */
void local_hst_svc_mapping::set_service(const std::string& hst,
                                        const std::string& svc,
                                        uint64_t host_id,
                                        uint64_t service_id,
                                        bool activated) {
  auto p_id = std::make_pair(host_id, service_id);
  _mapping[std::make_pair(hst, svc)] = p_id;
  _activated_mapping[p_id] = activated;
}

/**
 *  Get if the service is activated.
 *
 *  @param[in] hst_id       The host id.
 *  @param[in] service_id   The service id.
 *
 *  @return                 True if activated.
 */
bool local_hst_svc_mapping::get_activated(uint64_t hst_id,
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
void local_hst_svc_mapping::set_activated(uint64_t host_id,
                                          uint64_t service_id,
                                          bool activated) {
  _activated_mapping[std::make_pair(host_id, service_id)] = activated;
}

/**
 *  Get host ID by its name, from the global cache.
 *
 *  @param[in] hst  Host name.
 *
 *  @return Host ID, 0 if the cache does not know that host.
 */
uint64_t global_hst_svc_mapping::get_host_id(const std::string& hst) const {
  return config::applier::state::instance().cache().host_id(hst);
}

/**
 *  Get service ID by its name, from the global cache.
 *
 *  The convention of the local mapping is kept: (0, 0) says the couple was not
 *  resolved, and the caller decides what to make of it.
 *
 *  @param[in] hst  Host name.
 *  @param[in] svc  Service description.
 *
 *  @return Pair of integers with host ID and service ID, (0, 0) if the cache
 *          does not know that couple.
 */
std::pair<uint64_t, uint64_t> global_hst_svc_mapping::get_service_id(
    const std::string& hst,
    const std::string& svc) const {
  auto retval =
      config::applier::state::instance().cache().service_key(hst, svc);
  if (!retval.first || !retval.second)
    SPDLOG_LOGGER_DEBUG(
        _logger,
        "hst_svc_mapping: the global cache holds no service '{}' of host '{}'",
        svc, hst);
  return retval;
}

/**
 *  Get if the service is activated, that is: whether the global cache holds it.
 *
 *  A service the user has deactivated is not exported to the poller, so it is
 *  not in any stored configuration and the cache does not know it. Absence is
 *  therefore what deactivation looks like here, and it is the only signal
 *  available -- a configuration carries no activation flag of its own, every
 *  object it holds being one that runs.
 *
 *  That reading only holds for a KPI that names a service. The applier asks
 *  about every KPI it creates, and a boolean rule, a BA or a meta-service
 *  carries no service at all: both ids are then zero, there is nothing to look
 *  up, and answering "deactivated" would drop the KPI. Those are let through,
 *  as they always were.
 *
 *  It only holds, too, when the cache has something to say. It is filled from
 *  the stored configurations of the pollers, and a poller whose configuration
 *  is not there yet -- a first start, or one where the `.prot` files were lost
 *  -- is a poller the cache knows nothing about. Silence is then not a
 *  deactivation: concluding one would drop every service KPI of that poller,
 *  measured on a platform starting with an empty cache. The host is what tells
 *  the two apart, since it comes with the configuration its services belong
 *  to: no host, no answer.
 *
 *  @param[in] hst_id       The host id.
 *  @param[in] service_id   The service id.
 *
 *  @return                 True if the cache holds that service, if the KPI
 *                          names no service at all, or if the cache does not
 *                          know the host the service belongs to.
 */
bool global_hst_svc_mapping::get_activated(uint64_t hst_id,
                                           uint64_t service_id) const {
  if (!hst_id || !service_id)
    return true;
  auto& cache = config::applier::state::instance().cache();
  if (!cache.host(hst_id))
    return true;
  return cache.service(hst_id, service_id) != nullptr;
}
}  // namespace com::centreon::broker::bam
