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

#ifndef CCB_VICTORIA_METRICS_CONNECTOR_HH
#define CCB_VICTORIA_METRICS_CONNECTOR_HH

#include "com/centreon/broker/io/endpoint.hh"
#include "victoria_config.hh"

CCB_BEGIN()

namespace victoria_metrics {

class connector : public io::endpoint {
  std::shared_ptr<victoria_config> _conf;
  std::shared_ptr<persistent_cache> _cache;

 public:
  connector(const std::shared_ptr<victoria_config>& conf,
            const std::shared_ptr<persistent_cache>& cache);
  connector(const connector&) = delete;
  connector& operator=(const connector&) = delete;
  std::unique_ptr<io::stream> open() override;

  std::shared_ptr<victoria_config> get_conf() const { return _conf; }
};
}  // namespace victoria_metrics

CCB_END()

#endif  // !CCB_VICTORIA_METRICS_CONNECTOR_HH
