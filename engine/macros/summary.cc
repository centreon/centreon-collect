/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2026 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "engine/macros/summary.hh"

#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

namespace com::centreon::engine::macros {

namespace {
/**
 * @brief Tell whether the problem of a host or a service is handled: it is
 * acknowledged, in downtime or its active checks are disabled.
 *
 * @param n The host or the service.
 *
 * @return true if the problem is handled.
 */
bool problem_is_handled(const notifier& n) {
  return n.get_scheduled_downtime_depth() > 0 ||
         n.problem_has_been_acknowledged() || !n.active_checks_enabled();
}
}  // namespace

/**
 *  Compute all the summary macros ($TOTAL*$) and cache them in the macros,
 *  if not already done.
 *
 *  @param[in,out] mac The macros.
 */
void generate_summary_macros(macro_context* mac) {
  if (mac->summary)
    return;

  uint32_t hosts_up = 0;
  uint32_t hosts_down = 0;
  uint32_t hosts_down_unhandled = 0;
  uint32_t hosts_unreachable = 0;
  uint32_t hosts_unreachable_unhandled = 0;
  auto count_host = [&](const host& hst) {
    switch (hst.get_current_state()) {
      case host::state_up:
        if (hst.has_been_checked())
          hosts_up++;
        break;
      case host::state_down:
        hosts_down++;
        if (!problem_is_handled(hst))
          hosts_down_unhandled++;
        break;
      case host::state_unreachable:
        hosts_unreachable++;
        if (!problem_is_handled(hst))
          hosts_unreachable_unhandled++;
        break;
    }
  };

  uint32_t services_ok = 0;
  uint32_t services_warning = 0;
  uint32_t services_warning_unhandled = 0;
  uint32_t services_critical = 0;
  uint32_t services_critical_unhandled = 0;
  uint32_t services_unknown = 0;
  uint32_t services_unknown_unhandled = 0;
  auto count_service = [&](const service& svc) {
    const service::service_state state = svc.get_current_state();
    if (state == service::state_ok) {
      if (svc.has_been_checked())
        services_ok++;
      return;
    }
    // The problem of a service whose host is down or unreachable is handled.
    const host* hst = svc.get_host_ptr();
    const bool unhandled =
        !problem_is_handled(svc) &&
        !(hst && (hst->get_current_state() == host::state_down ||
                  hst->get_current_state() == host::state_unreachable));
    switch (state) {
      case service::state_warning:
        services_warning++;
        if (unhandled)
          services_warning_unhandled++;
        break;
      case service::state_critical:
        services_critical++;
        if (unhandled)
          services_critical_unhandled++;
        break;
      case service::state_unknown:
        services_unknown++;
        if (unhandled)
          services_unknown_unhandled++;
        break;
      default:
        break;
    }
  };

  if (mac->contact_ptr) {
    // The totals are filtered by contact: only its objects are counted.
    const contact::notified_objects& objects =
        mac->contact_ptr->get_notified_objects();
    for (const host* hst : objects.hosts)
      count_host(*hst);
    for (const service* svc : objects.services)
      count_service(*svc);
  } else {
    for (const auto& [name, hst] : host::hosts)
      count_host(*hst);
    for (const auto& [key, svc] : service::services)
      count_service(*svc);
  }

  // These macros are time-intensive to compute, and will likely be
  // reused, so save them all for future use, in the order of the macro ids.
  mac->summary = std::array<uint32_t, MACRO_SUMMARY_COUNT>{
      hosts_up,
      hosts_down,
      hosts_unreachable,
      hosts_down_unhandled,
      hosts_unreachable_unhandled,
      hosts_down + hosts_unreachable,
      hosts_down_unhandled + hosts_unreachable_unhandled,
      services_ok,
      services_warning,
      services_critical,
      services_unknown,
      services_warning_unhandled,
      services_critical_unhandled,
      services_unknown_unhandled,
      services_warning + services_critical + services_unknown,
      services_warning_unhandled + services_critical_unhandled +
          services_unknown_unhandled};
}

}  // namespace com::centreon::engine::macros
