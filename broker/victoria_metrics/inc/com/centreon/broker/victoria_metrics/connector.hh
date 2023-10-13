/**
 * Copyright 2022-2023 Centreon
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

#ifndef CCB_VICTORIA_METRICS_CONNECTOR_HH
#define CCB_VICTORIA_METRICS_CONNECTOR_HH

#include "com/centreon/broker/http_tsdb/http_tsdb_config.hh"
#include "com/centreon/broker/io/endpoint.hh"

namespace com::centreon::broker {

namespace victoria_metrics {

class connector : public io::endpoint {
  std::shared_ptr<http_tsdb::http_tsdb_config> _conf;
  std::string _account_id;

 public:
  connector(const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
            const std::string& account_id);
  connector(const connector&) = delete;
  connector& operator=(const connector&) = delete;
  std::shared_ptr<io::stream> open() override;

  std::shared_ptr<http_tsdb::http_tsdb_config> get_conf() const {
    return _conf;
  }

  const std::string& get_account_id() const { return _account_id; }
};
}  // namespace victoria_metrics

}  // namespace com::centreon::broker

#endif  // !CCB_VICTORIA_METRICS_CONNECTOR_HH
