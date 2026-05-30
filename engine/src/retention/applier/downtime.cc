/**
 * Copyright 2011-2021 Centreon
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
#include "com/centreon/engine/retention/applier/downtime.hh"
#include "com/centreon/engine/globals.hh"
#include "common/downtimes/downtime_manager.hh"

namespace downtimes = com::centreon::common::downtimes;
using namespace com::centreon::engine;
using namespace com::centreon::engine::retention;

/**
 *  Add downtimes on appropriate hosts and services.
 *
 *  @param[in] lst The downtime list to add.
 */
void applier::downtime::apply(list_downtime const& lst) {
  for (const auto& dt : lst) {
    bool is_service = (dt->downtime_type() == retention::downtime::service);

    auto found_host = host::hosts.find(dt->host_name());
    if (found_host == host::hosts.end()) {
      downtimes_logger->error(
          "Cannot add downtime on host '{}' because it does not exist",
          dt->host_name());
      continue;
    }

    uint64_t host_id = found_host->second->host_id();
    uint64_t service_id = 0;

    if (is_service) {
      auto found_svc = service::services.find(
          {dt->host_name(), dt->service_description()});
      if (found_svc == service::services.end()) {
        downtimes_logger->error(
            "Cannot create service downtime on service ('{}', '{}') because "
            "it does not exist",
            dt->host_name(), dt->service_description());
        continue;
      }
      service_id = found_svc->second->service_id();
    }

    auto d = std::make_shared<downtimes::downtime>(
        host_id, service_id, dt->entry_time(), dt->author(),
        dt->comment_data(), dt->start_time(), dt->end_time(), dt->fixed(),
        dt->triggered_by(), dt->duration(), dt->downtime_id(), downtimes_logger);
    downtimes::downtime_manager::instance().add_downtime(d);
    d->notify_broker_load();
    d->subscribe();
  }
}
