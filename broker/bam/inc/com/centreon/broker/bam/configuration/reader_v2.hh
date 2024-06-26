/**
 * Copyright 2014-2016, 2022-2024 Centreon
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

#ifndef CCB_BAM_CONFIGURATION_READER_V2_HH
#define CCB_BAM_CONFIGURATION_READER_V2_HH

#include "com/centreon/broker/bam/configuration/state.hh"

#include "com/centreon/broker/sql/database_config.hh"

namespace com::centreon::broker {

// Forward declaration.
class mysql;

namespace bam::configuration {
/**
 *  @class reader_v2 reader_v2.hh
 * "com/centreon/broker/bam/configuration/reader_v2.hh"
 *  @brief Using the dbinfo to access the database, load state_obj
 *         with configuration.
 *
 *  Extract the database content to a configuration state usable by
 *  the BAM engine.
 */
class reader_v2 {
  std::shared_ptr<spdlog::logger> _logger;
  mysql& _mysql;
  database_config _storage_cfg;

  reader_v2(reader_v2 const& other);
  reader_v2& operator=(reader_v2 const& other);
  void _load(state::kpis& kpis);
  void _load(state::bas& bas, ba_svc_mapping& mapping);
  void _load(state::bool_exps& bool_exps);
  // void _load(state::meta_services& meta_services);
  void _load(bam::hst_svc_mapping& mapping);
  void _load_dimensions();

 public:
  reader_v2(mysql& centreon_db, const database_config& storage_cfg);
  ~reader_v2() noexcept = default;
  void read(state& state_obj);
};
}  // namespace bam::configuration

}  // namespace com::centreon::broker

#endif  // !CCB_BAM_CONFIGURATION_READER_V2_HH
