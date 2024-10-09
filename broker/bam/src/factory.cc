/**
 * Copyright 2014-2016, 2021-2024 Centreon
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

#include "com/centreon/broker/bam/factory.hh"

#include <absl/strings/match.h>
#include "com/centreon/broker/bam/connector.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

/**
 *  Check if a configuration match the BAM layer.
 *
 *  @param[in] cfg  Endpoint configuration.
 *
 *  @return True if the configuration matches the BAM layer.
 */
bool factory::has_endpoint(config::endpoint& cfg, io::extension* ext) {
  if (ext)
    *ext = io::extension("BAM", false, false);
  bool is_bam{absl::EqualsIgnoreCase("bam", cfg.type)};
  bool is_bam_bi{absl::EqualsIgnoreCase("bam_bi", cfg.type)};
  if (is_bam || is_bam_bi)
    cfg.read_timeout = 1;

  if (is_bam)
    cfg.cache_enabled = true;

  return is_bam || is_bam_bi;
}

/**
 *  Build a BAM endpoint from a configuration.
 *
 *  @param[in]  cfg         Endpoint configuration.
 *  @param[out] is_acceptor Will be set to false.
 *  @param[in]  cache       The persistent cache.
 *
 *  @return Endpoint matching the given configuration.
 */
io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params [[maybe_unused]],
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache) const {
  // Find DB parameters.
  database_config db_cfg(cfg, global_params);

  // Is it a BAM or BAM-BI output ?
  bool is_bam_bi{absl::EqualsIgnoreCase(cfg.type, "bam_bi")};

  // External command file.
  std::string ext_cmd_file;
  if (!is_bam_bi) {
    auto it = cfg.params.find("command_file");
    if (it == cfg.params.end() || it->second.empty())
      throw msg_fmt("BAM: command_file parameter not set");
    ext_cmd_file = it->second;
  }

  // Storage database name.
  std::string storage_db_name;
  {
    auto it = cfg.params.find("storage_db_name");
    if (it != cfg.params.end())
      storage_db_name = it->second;
  }

  // Connector.
  std::unique_ptr<bam::connector> c;
  if (is_bam_bi)
    c = connector::create_reporting_connector(db_cfg);
  else
    c = connector::create_monitoring_connector(ext_cmd_file, db_cfg,
                                               storage_db_name, cache);
  is_acceptor = false;
  return c.release();
}
