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

#ifndef CCB_BAM_HST_SVC_MAPPING_HH
#define CCB_BAM_HST_SVC_MAPPING_HH

#include <absl/container/flat_hash_map.h>

#include "common/log_v2/log_v2.hh"

namespace com::centreon::broker::bam {

using com::centreon::common::log_v2::log_v2;

/**
 *  @class hst_svc_mapping hst_svc_mapping.hh
 * "com/centreon/broker/bam/hst_svc_mapping.hh"
 *  @brief Link name to ID.
 *
 *  Allow to find an ID of a host or service by its name, and tell whether a
 *  service is activated.
 *
 *  Where those answers come from depends on how the platform is configured, so
 *  the two regimes are two implementations, exclusive of one another: the
 *  stored configurations of the pollers when Broker holds them, the
 *  configuration database otherwise.
 */
class hst_svc_mapping {
 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  hst_svc_mapping(const std::shared_ptr<spdlog::logger>& logger)
      : _logger{logger} {}
  virtual ~hst_svc_mapping() noexcept = default;
  hst_svc_mapping(const hst_svc_mapping&) = delete;
  hst_svc_mapping& operator=(const hst_svc_mapping&) = delete;

  virtual uint64_t get_host_id(const std::string& hst) const = 0;
  virtual std::pair<uint64_t, uint64_t> get_service_id(
      const std::string& hst,
      const std::string& svc) const = 0;
  virtual bool get_activated(uint64_t hst_id, uint64_t service_id) const = 0;
};

/**
 *  @class local_hst_svc_mapping hst_svc_mapping.hh
 * "com/centreon/broker/bam/hst_svc_mapping.hh"
 *  @brief The mapping BAM builds for itself, from the configuration database.
 *
 *  Used when Broker does not hold the configurations of the pollers. The
 *  reader fills it with the couples the boolean expressions name and the
 *  activation of the services the KPIs point at, and nothing else.
 */
class local_hst_svc_mapping : public hst_svc_mapping {
  absl::flat_hash_map<std::pair<std::string, std::string>,
                      std::pair<uint64_t, uint64_t>>
      _mapping;

  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, bool> _activated_mapping;

 public:
  using hst_svc_mapping::hst_svc_mapping;

  uint64_t get_host_id(const std::string& hst) const override;
  std::pair<uint64_t, uint64_t> get_service_id(
      const std::string& hst,
      const std::string& svc) const override;
  bool get_activated(uint64_t hst_id, uint64_t service_id) const override;

  void set_host(const std::string& hst, uint64_t host_id);
  void set_service(const std::string& hst,
                   const std::string& svc,
                   uint64_t host_id,
                   uint64_t service_id,
                   bool activated);
  void set_activated(uint64_t host_id, uint64_t service_id, bool activated);
};

/**
 *  @class global_hst_svc_mapping hst_svc_mapping.hh
 * "com/centreon/broker/bam/hst_svc_mapping.hh"
 *  @brief The answers taken from the global cache, without a copy.
 *
 *  Used when Broker holds the configurations of the pollers: the cache is then
 *  filled from them at startup and follows every acknowledged difference, so
 *  it already knows what BAM would otherwise ask the database for. Nothing is
 *  loaded, nothing is kept: each question is answered by the cache itself.
 */
class global_hst_svc_mapping : public hst_svc_mapping {
 public:
  using hst_svc_mapping::hst_svc_mapping;

  uint64_t get_host_id(const std::string& hst) const override;
  std::pair<uint64_t, uint64_t> get_service_id(
      const std::string& hst,
      const std::string& svc) const override;
  bool get_activated(uint64_t hst_id, uint64_t service_id) const override;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_HST_SVC_MAPPING_HH
