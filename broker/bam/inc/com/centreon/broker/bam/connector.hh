/*
** Copyright 2014-2015, 2020-2023 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_BAM_CONNECTOR_HH
#define CCB_BAM_CONNECTOR_HH

#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/sql/database_config.hh"

namespace com::centreon::broker::bam {
/**
 *  @class connector connector.hh "com/centreon/broker/bam/connector.hh"
 *  @brief Connect to a database.
 *
 *  Send perfdata in a Centreon bam database.
 */
class connector : public io::endpoint {
  enum stream_type { bam_monitoring_type = 1, bam_reporting_type };

  const stream_type _type;
  const database_config _db_cfg;
  std::string _ext_cmd_file;
  std::string _storage_db_name;
  std::shared_ptr<persistent_cache> _cache;

  connector(stream_type type, const database_config& db_cfg,
            const multiplexing::muxer_filter& filter);

 public:
  static std::unique_ptr<connector> create_monitoring_connector(
      const std::string& ext_cmd_file, const database_config& db_cfg,
      const std::string& storage_db_name,
      std::shared_ptr<persistent_cache> cache);

  static std::unique_ptr<connector> create_reporting_connector(
      const database_config& db_cfg);
  ~connector() noexcept = default;
  connector() = delete;
  connector(const connector&) = delete;
  connector& operator=(const connector&) = delete;
  void connect_monitoring(std::string const& ext_cmd_file,
                          database_config const& db_cfg,
                          std::string const& storage_db_name,
                          std::shared_ptr<persistent_cache> cache);
  void connect_reporting(database_config const& db_cfg);
  std::shared_ptr<io::stream> open() override;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_CONNECTOR_HH
