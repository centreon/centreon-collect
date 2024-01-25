/*
 * Copyright 2023 Centreon
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

#ifndef CCB_STATS_EXPORTER_EXPORTER_HTTP_HH
#define CCB_STATS_EXPORTER_EXPORTER_HTTP_HH

#include "com/centreon/broker/stats_exporter/exporter.hh"

namespace com::centreon::broker {

namespace stats_exporter {
/**
 * @brief Export stats to an opentelemetry collector with http protocol.
 * The work is essentially done by the exporter class, but the url is defined
 * here and is usually of the form: http://XX.XX.XX.XX:4318/v1/metrics
 *
 */
class exporter_http : public exporter {
  const std::string _url;

 public:
  exporter_http(const std::string& url, const config::state& s);
  ~exporter_http() noexcept = default;
};

}  // namespace stats_exporter

}

#endif /* !CCB_STATS_EXPORTER_EXPORTER_HTTP_HH */
