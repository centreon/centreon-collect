/**
 * Copyright 2014-2015, 2022-2024 Centreon
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

#ifndef CCB_BAM_CONFIGURATION_APPLIER_BA_HH
#define CCB_BAM_CONFIGURATION_APPLIER_BA_HH

#include "com/centreon/broker/bam/ba.hh"
#include "com/centreon/broker/bam/configuration/ba.hh"
#include "com/centreon/broker/bam/configuration/state.hh"
#include "com/centreon/broker/bam/service_book.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/persistent_cache.hh"

namespace com::centreon::broker {

// Forward declarations.
namespace neb {
class host;
class service;
}  // namespace neb

namespace bam::configuration::applier {
/**
 *  @class ba ba.hh "com/centreon/broker/bam/configuration/applier/ba.hh"
 *  @brief Apply BA configuration.
 *
 *  Take the configuration of BAs and apply it.
 */
class ba {
  std::shared_ptr<spdlog::logger> _logger;
  struct applied {
    configuration::ba cfg;
    std::shared_ptr<bam::ba> obj;
  };
  std::map<uint32_t, applied> _applied;

  std::shared_ptr<neb::host> _ba_host(uint32_t host_id);
  std::shared_ptr<neb::pb_host> _ba_pb_host(uint32_t host_id);
  std::shared_ptr<neb::service> _ba_service(uint32_t ba_id,
                                            uint32_t host_id,
                                            uint32_t service_id,
                                            bool in_downtime = false);
  std::shared_ptr<neb::pb_service> _ba_pb_service(uint32_t ba_id,
                                                  uint32_t host_id,
                                                  const std::string& host_name,
                                                  uint32_t service_id,
                                                  bool in_downtime = false);
  void _internal_copy(ba const& other);
  std::shared_ptr<bam::ba> _new_ba(configuration::ba const& cfg,
                                   service_book& book);

 public:
  ba(const std::shared_ptr<spdlog::logger>& logger);
  ba(const ba& other);
  ~ba();
  ba& operator=(ba const& other);
  void apply(configuration::state::bas const& my_bas, service_book& book);
  std::shared_ptr<bam::ba> find_ba(uint32_t id) const;
  void visit(io::stream* visitor);
  void save_to_cache(persistent_cache& cache);
  void apply_inherited_downtime(const inherited_downtime& dwn);
  void apply_inherited_downtime(const pb_inherited_downtime& dwn);
};
}  // namespace bam::configuration::applier

}  // namespace com::centreon::broker

#endif  // !CCB_BAM_CONFIGURATION_APPLIER_BA_HH
