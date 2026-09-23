/**
 * Copyright 2026 Centreon
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

#include "com/centreon/broker/otlp/stream.hh"
#include <spdlog/spdlog.h>

#include "bbdo/neb.pb.h"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/neb/internal.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;

stream::stream(const otlp_config::pointer& conf,
               const std::shared_ptr<spdlog::logger>& logger)
    : io::stream("otlp"), _conf(conf), _logger(logger) {}

bool stream::read(std::shared_ptr<io::data>& d,
                  time_t deadline [[maybe_unused]]) {
  d.reset();
  throw exceptions::shutdown("cannot read from OTLP stream");
}

int stream::write(std::shared_ptr<io::data> const& d) {
  SPDLOG_LOGGER_TRACE(_logger, "OTLP: event category:{}, element:{}",
                      category_of_type(d->type()), element_of_type(d->type()));
  return 0;
}

int stream::flush() {
  SPDLOG_LOGGER_TRACE(_logger, "OTPL: stream flush");
  return 0;
}

int32_t stream::stop() {
  SPDLOG_LOGGER_TRACE(_logger, "OTPL: stream Try to stop");

  return 0;
}

void stream::statistics(nlohmann::json& tree) const {}
