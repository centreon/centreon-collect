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
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/downtimes/host_downtime.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/log_v2.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::retention;

/**
 *  Add downtimes on appropriate hosts and services.
 *
 *  @param[in] lst The downtime list to add.
 */
void applier::downtime::apply(list_downtime const& lst) {
  // Big speedup when reading retention.dat in bulk.

  for (list_downtime::const_iterator it(lst.begin()), end(lst.end()); it != end;
       ++it) {
    if ((*it)->downtime_type() == retention::downtime::host)
      _add_host_downtime(**it);
    else
      _add_service_downtime(**it);
  }
}

/**
 *  Add host downtime.
 *
 *  @param[in] obj The downtime to add into the host.
 */
void applier::downtime::_add_host_downtime(
    retention::downtime const& obj) noexcept {
  auto found = host::hosts.find(obj.host_name());
  if (found != host::hosts.end()) {
    auto dt{std::make_shared<downtimes::host_downtime>(
        found->second->host_id(), obj.entry_time(), obj.author(),
        obj.comment_data(), obj.start_time(), obj.end_time(), obj.fixed(),
        obj.triggered_by(), obj.duration(), obj.downtime_id())};
    downtimes::downtime_manager::instance().add_downtime(dt);
    dt->schedule();
    downtimes::downtime_manager::instance().register_downtime(
        downtimes::downtime::host_downtime, obj.downtime_id());
  } else
    log_v2::downtimes()->error(
        "Cannot add host downtime on host '{}' because it does not exist",
        obj.host_name());
}

/**
 *  Add service downtime.
 *
 *  @param[in] obj The downtime to add into the service.
 */
void applier::downtime::_add_service_downtime(
    retention::downtime const& obj) noexcept {
  auto found =
      service::services.find({obj.host_name(), obj.service_description()});
  if (found != service::services.end()) {
    auto dt{std::make_shared<downtimes::service_downtime>(
        found->second->host_id(), found->second->service_id(), obj.entry_time(),
        obj.author(), obj.comment_data(), obj.start_time(), obj.end_time(),
        obj.fixed(), obj.triggered_by(), obj.duration(), obj.downtime_id())};
    downtimes::downtime_manager::instance().add_downtime(dt);
    dt->schedule();
    downtimes::downtime_manager::instance().register_downtime(
        downtimes::downtime::service_downtime, obj.downtime_id());
  } else
    log_v2::downtimes()->error(
        "Cannot create service downtime on service ('{}', '{}') because it "
        "does not exist",
        obj.host_name(), obj.service_description());
}
