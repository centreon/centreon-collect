/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/engine/configuration/applier/recurringdowntime.hh"

#include "com/centreon/engine/downtimes/recurring_downtime.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using com::centreon::exceptions::msg_fmt;
using com::centreon::engine::downtimes::recurring_downtime;

/**
 * @brief Add a new recurring downtime rule.
 *
 * @param obj The new rule.
 */
void applier::recurringdowntime::add_object(
    const configuration::Recurringdowntime& obj) {
  config_logger->debug("Creating new recurring downtime #{}.",
                       obj.downtime_id());

  auto rule = std::make_shared<recurring_downtime>(
      obj, recurring_downtime::not_before_for_new_rule());
  if (!recurring_downtime::recurring_downtimes
           .emplace(obj.downtime_id(), std::move(rule))
           .second)
    throw msg_fmt("Recurring downtime #{} is defined twice", obj.downtime_id());

  pb_config.add_recurringdowntimes()->CopyFrom(obj);
}

/**
 * @brief Modify a recurring downtime rule: its downtimes that have not started
 * yet are deleted, the next run schedules the new ones.
 *
 * @param to_modify The current configuration of the rule.
 * @param new_object The new configuration of the rule.
 */
void applier::recurringdowntime::modify_object(
    configuration::Recurringdowntime* to_modify,
    const configuration::Recurringdowntime& new_object) {
  config_logger->debug("Modifying recurring downtime #{}.",
                       new_object.downtime_id());

  auto it =
      recurring_downtime::recurring_downtimes.find(new_object.downtime_id());
  if (it == recurring_downtime::recurring_downtimes.end())
    throw msg_fmt("Could not modify non-existing recurring downtime #{}",
                  new_object.downtime_id());

  recurring_downtime::delete_future_downtimes(new_object.downtime_id(),
                                              time(nullptr));
  it->second = std::make_shared<recurring_downtime>(
      new_object, recurring_downtime::not_before_for_new_rule());
  to_modify->CopyFrom(new_object);
}

/**
 * @brief Remove a recurring downtime rule and its downtimes that have not
 * started yet.
 *
 * @param idx The index of the rule in pb_config.recurringdowntimes().
 */
void applier::recurringdowntime::remove_object(ssize_t idx) {
  const configuration::Recurringdowntime& obj =
      pb_config.recurringdowntimes().at(idx);
  config_logger->debug("Removing recurring downtime #{}.", obj.downtime_id());

  recurring_downtime::delete_future_downtimes(obj.downtime_id(),
                                              time(nullptr));
  recurring_downtime::recurring_downtimes.erase(obj.downtime_id());
  pb_config.mutable_recurringdowntimes()->DeleteSubrange(idx, 1);
}

/**
 * @brief Keep the targets of a rule that this Engine knows; the other ones are
 * a warning, not an error: Centreon may send a rule whose target is disabled
 * or on another poller.
 *
 * @param obj The rule to resolve.
 * @param err The configuration error counter.
 */
void applier::recurringdowntime::resolve_object(
    const configuration::Recurringdowntime& obj,
    error_cnt& err) {
  auto it = recurring_downtime::recurring_downtimes.find(obj.downtime_id());
  if (it == recurring_downtime::recurring_downtimes.end())
    throw msg_fmt("Cannot resolve non-existing recurring downtime #{}",
                  obj.downtime_id());

  for (const auto& p : obj.periods())
    if (!p.fixed() && p.duration() == 0) {
      config_logger->warn(
          "Warning: recurring downtime #{} has a flexible period without "
          "duration: it schedules no downtime",
          obj.downtime_id());
      ++err.config_warnings;
    }

  std::vector<uint64_t> host_ids;
  for (uint64_t host_id : obj.host_ids()) {
    if (host::hosts_by_id.find(host_id) == host::hosts_by_id.end()) {
      config_logger->warn(
          "Warning: recurring downtime #{} targets host {} that does not "
          "exist: ignored",
          obj.downtime_id(), host_id);
      ++err.config_warnings;
    } else
      host_ids.push_back(host_id);
  }

  std::vector<std::pair<uint64_t, uint64_t>> service_ids;
  for (const auto& s : obj.service_ids()) {
    if (service::services_by_id.find({s.host_id(), s.service_id()}) ==
        service::services_by_id.end()) {
      config_logger->warn(
          "Warning: recurring downtime #{} targets service {}/{} that does "
          "not exist: ignored",
          obj.downtime_id(), s.host_id(), s.service_id());
      ++err.config_warnings;
    } else
      service_ids.emplace_back(s.host_id(), s.service_id());
  }

  it->second->set_targets(std::move(host_ids), std::move(service_ids));
}
