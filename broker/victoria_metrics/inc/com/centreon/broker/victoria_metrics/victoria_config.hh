/*
** Copyright 2022 Centreon
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

#ifndef CCB_VICTORIA_METRICS_CONFIG_HH
#define CCB_VICTORIA_METRICS_CONFIG_HH

#include "com/centreon/broker/http_tsdb/http_tsdb_config.hh"

CCB_BEGIN()

namespace victoria_metrics {
class victoria_config : public http_tsdb::http_tsdb_config {
  std::string _http_target;

 public:
  victoria_config(const std::string& http_target) : _http_target(http_target) {}

  const std::string& get_http_target() const { return _http_target; }
};

}  // namespace victoria_metrics

CCB_END()

#endif  // !CCB_VICTORIA_METRICS_CONFIG_HH
