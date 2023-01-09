/*
** Copyright 2009-2022 Centreon
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

#include "com/centreon/broker/neb/callbacks.hh"

#include <absl/strings/str_split.h>
#include <unistd.h>

#include "bbdo/neb.pb.h"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/neb/callback.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/neb/initial.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/set_log_data.hh"
#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/hostdependency.hh"
#include "com/centreon/engine/hostgroup.hh"
#include "com/centreon/engine/nebcallbacks.hh"
#include "com/centreon/engine/nebstructs.hh"
#include "com/centreon/engine/servicedependency.hh"
#include "com/centreon/engine/servicegroup.hh"
#include "com/centreon/engine/severity.hh"
#include "com/centreon/engine/tag.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;

// List of Nagios modules.
extern nebmodule* neb_module_list;

// Acknowledgement list.
std::unordered_map<std::pair<uint32_t, uint32_t>,
                   neb::acknowledgement,
                   absl::Hash<std::pair<uint32_t, uint32_t>>>
    neb::gl_acknowledgements;

// Downtime list.
struct private_downtime_params {
  bool cancelled;
  time_t deletion_time;
  time_t end_time;
  bool started;
  time_t start_time;
};
// Unstarted downtimes.
static std::unordered_map<uint32_t, private_downtime_params> downtimes;

// Load flags.
int neb::gl_mod_flags(0);

// Module handle.
void* neb::gl_mod_handle(nullptr);

// List of common callbacks.
static struct {
  uint32_t macro;
  int (*callback)(int, void*);
} const gl_callbacks[] = {
    {NEBCALLBACK_ACKNOWLEDGEMENT_DATA, &neb::callback_acknowledgement},
    {NEBCALLBACK_COMMENT_DATA, &neb::callback_comment},
    {NEBCALLBACK_DOWNTIME_DATA, &neb::callback_downtime},
    {NEBCALLBACK_EVENT_HANDLER_DATA, &neb::callback_event_handler},
    {NEBCALLBACK_EXTERNAL_COMMAND_DATA, &neb::callback_external_command},
    {NEBCALLBACK_FLAPPING_DATA, &neb::callback_flapping_status},
    {NEBCALLBACK_HOST_CHECK_DATA, &neb::callback_host_check},
    {NEBCALLBACK_HOST_STATUS_DATA, &neb::callback_host_status},
    {NEBCALLBACK_PROGRAM_STATUS_DATA, &neb::callback_program_status},
    {NEBCALLBACK_SERVICE_CHECK_DATA, &neb::callback_service_check},
    {NEBCALLBACK_SERVICE_STATUS_DATA, &neb::callback_service_status},
    {NEBCALLBACK_ADAPTIVE_SEVERITY_DATA, &neb::callback_severity},
    {NEBCALLBACK_ADAPTIVE_TAG_DATA, &neb::callback_tag}};

// List of common callbacks.
static struct {
  uint32_t macro;
  int (*callback)(int, void*);
} const gl_pb_callbacks[] = {
    {NEBCALLBACK_ACKNOWLEDGEMENT_DATA, &neb::callback_acknowledgement},
    {NEBCALLBACK_COMMENT_DATA, &neb::callback_comment},
    {NEBCALLBACK_DOWNTIME_DATA, &neb::callback_downtime},
    {NEBCALLBACK_EVENT_HANDLER_DATA, &neb::callback_event_handler},
    {NEBCALLBACK_EXTERNAL_COMMAND_DATA, &neb::callback_external_command},
    {NEBCALLBACK_FLAPPING_DATA, &neb::callback_flapping_status},
    {NEBCALLBACK_HOST_CHECK_DATA, &neb::callback_host_check},
    {NEBCALLBACK_HOST_STATUS_DATA, &neb::callback_pb_host_status},
    {NEBCALLBACK_PROGRAM_STATUS_DATA, &neb::callback_program_status},
    {NEBCALLBACK_SERVICE_CHECK_DATA, &neb::callback_service_check},
    {NEBCALLBACK_SERVICE_STATUS_DATA, &neb::callback_pb_service_status},
    {NEBCALLBACK_ADAPTIVE_SEVERITY_DATA, &neb::callback_severity},
    {NEBCALLBACK_ADAPTIVE_TAG_DATA, &neb::callback_tag}};

// List of Engine-specific callbacks.
static struct {
  uint32_t macro;
  int (*callback)(int, void*);
} const gl_engine_callbacks[] = {
    {NEBCALLBACK_ADAPTIVE_DEPENDENCY_DATA, &neb::callback_dependency},
    {NEBCALLBACK_ADAPTIVE_HOST_DATA, &neb::callback_host},
    {NEBCALLBACK_ADAPTIVE_SERVICE_DATA, &neb::callback_service},
    {NEBCALLBACK_CUSTOM_VARIABLE_DATA, &neb::callback_custom_variable},
    {NEBCALLBACK_GROUP_DATA, &neb::callback_group},
    {NEBCALLBACK_GROUP_MEMBER_DATA, &neb::callback_group_member},
    {NEBCALLBACK_MODULE_DATA, &neb::callback_module},
    {NEBCALLBACK_RELATION_DATA, &neb::callback_relation}};

static struct {
  uint32_t macro;
  int (*callback)(int, void*);
} const gl_pb_engine_callbacks[] = {
    {NEBCALLBACK_ADAPTIVE_DEPENDENCY_DATA, &neb::callback_dependency},
    {NEBCALLBACK_ADAPTIVE_HOST_DATA, &neb::callback_pb_host},
    {NEBCALLBACK_ADAPTIVE_SERVICE_DATA, &neb::callback_pb_service},
    {NEBCALLBACK_CUSTOM_VARIABLE_DATA, &neb::callback_custom_variable},
    {NEBCALLBACK_GROUP_DATA, &neb::callback_group},
    {NEBCALLBACK_GROUP_MEMBER_DATA, &neb::callback_group_member},
    {NEBCALLBACK_MODULE_DATA, &neb::callback_module},
    {NEBCALLBACK_RELATION_DATA, &neb::callback_relation}};

// Registered callbacks.
std::list<std::unique_ptr<neb::callback>> neb::gl_registered_callbacks;

// External function to get program version.
extern "C" {
char const* get_program_version();
}

/**
 *  @brief Function that process acknowledgement data.
 *
 *  This function is called by Nagios when some acknowledgement data are
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_ACKNOWLEDGEMENT_DATA).
 *  @param[in] data          A pointer to a nebstruct_acknowledgement_data
 *                           containing the acknowledgement data.
 *
 *  @return 0 on success.
 */
int neb::callback_acknowledgement(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating acknowledgement event");
  (void)callback_type;

  try {
    // In/Out variables.
    nebstruct_acknowledgement_data const* ack_data;
    auto ack{std::make_shared<neb::acknowledgement>()};

    // Fill output var.
    ack_data = static_cast<nebstruct_acknowledgement_data*>(data);
    ack->acknowledgement_type = ack_data->acknowledgement_type;
    if (ack_data->author_name)
      ack->author = misc::string::check_string_utf8(ack_data->author_name);
    if (ack_data->comment_data)
      ack->comment = misc::string::check_string_utf8(ack_data->comment_data);
    ack->entry_time = time(nullptr);
    if (!ack_data->host_id)
      throw msg_fmt("unnamed host");
    if (ack_data->service_id) {
      ack->host_id = ack_data->host_id;
      ack->service_id = ack_data->service_id;
      if (!ack->host_id || !ack->service_id)
        throw msg_fmt(
            "acknowledgement on service with host_id or service_id 0");
    } else {
      ack->host_id = ack_data->host_id;
      if (ack->host_id == 0)
        throw msg_fmt("acknowledgement on host with id 0");
    }
    ack->poller_id = config::applier::state::instance().poller_id();
    ack->is_sticky = ack_data->is_sticky;
    ack->notify_contacts = ack_data->notify_contacts;
    ack->persistent_comment = ack_data->persistent_comment;
    ack->state = ack_data->state;
    gl_acknowledgements[std::make_pair(ack->host_id, ack->service_id)] = *ack;

    // Send event.
    gl_publisher.write(ack);
  } catch (std::exception const& e) {
    log_v2::neb()->error(
        "callbacks: error occurred while generating acknowledgement event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process comment data.
 *
 *  This function is called by Nagios when some comment data are available.
 *
 *  @param[in] callback_type Type of the callback (NEBCALLBACK_COMMENT_DATA).
 *  @param[in] data          A pointer to a nebstruct_comment_data containing
 *                           the comment data.
 *
 *  @return 0 on success.
 */
int neb::callback_comment(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating comment event");
  (void)callback_type;

  try {
    // In/Out variables.
    nebstruct_comment_data const* comment_data;
    auto comment{std::make_shared<neb::comment>()};

    // Fill output var.
    comment_data = static_cast<nebstruct_comment_data*>(data);
    if (comment_data->author_name)
      comment->author =
          misc::string::check_string_utf8(comment_data->author_name);
    if (comment_data->comment_data)
      comment->data =
          misc::string::check_string_utf8(comment_data->comment_data);
    comment->comment_type = comment_data->comment_type;
    if (NEBTYPE_COMMENT_DELETE == comment_data->type)
      comment->deletion_time = time(nullptr);
    comment->entry_time = comment_data->entry_time;
    comment->entry_type = comment_data->entry_type;
    comment->expire_time = comment_data->expire_time;
    comment->expires = comment_data->expires;
    if (comment_data->service_id) {
      comment->host_id = comment_data->host_id;
      comment->service_id = comment_data->service_id;
      if (!comment->host_id || !comment->service_id)
        throw msg_fmt(
            "comment created from a service with host_id/service_id 0");
    } else {
      comment->host_id = comment_data->host_id;
      if (comment->host_id == 0)
        throw msg_fmt("comment created from a host with host_id 0");
    }
    comment->poller_id = config::applier::state::instance().poller_id();
    comment->internal_id = comment_data->comment_id;
    comment->persistent = comment_data->persistent;
    comment->source = comment_data->source;

    // Send event.
    gl_publisher.write(comment);
  } catch (std::exception const& e) {
    log_v2::neb()->error(
        "callbacks: error occurred while generating comment event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process custom variable data.
 *
 *  This function is called by Engine when some custom variable data is
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_CUSTOMVARIABLE_DATA).
 *  @param[in] data          Pointer to a nebstruct_custom_variable_data
 *                           containing the custom variable data.
 *
 *  @return 0 on success.
 */
int neb::callback_custom_variable(int callback_type, void* data) {
  // Log message.

  log_v2::neb()->info("callbacks: generating custom variable event");
  (void)callback_type;

  try {
    // Input variable.
    nebstruct_custom_variable_data const* cvar(
        static_cast<nebstruct_custom_variable_data*>(data));
    if (cvar && cvar->var_name && cvar->var_value) {
      // Host custom variable.
      if (NEBTYPE_HOSTCUSTOMVARIABLE_ADD == cvar->type) {
        engine::host* hst(static_cast<engine::host*>(cvar->object_ptr));
        if (hst && !hst->name().empty()) {
          // Fill custom variable event.
          uint64_t host_id = engine::get_host_id(hst->name());
          if (host_id != 0) {
            auto new_cvar{std::make_shared<custom_variable>()};
            new_cvar->enabled = true;
            new_cvar->host_id = host_id;
            new_cvar->modified = false;
            new_cvar->name = misc::string::check_string_utf8(cvar->var_name);
            new_cvar->var_type = 0;
            new_cvar->update_time = cvar->timestamp.tv_sec;
            new_cvar->value = misc::string::check_string_utf8(cvar->var_value);
            new_cvar->default_value =
                misc::string::check_string_utf8(cvar->var_value);

            // Send custom variable event.
            log_v2::neb()->info(
                "callbacks: new custom variable '{}' on host {}",
                new_cvar->name, new_cvar->host_id);
            neb::gl_publisher.write(new_cvar);
          }
        }
      } else if (NEBTYPE_HOSTCUSTOMVARIABLE_DELETE == cvar->type) {
        engine::host* hst(static_cast<engine::host*>(cvar->object_ptr));
        if (hst && !hst->name().empty()) {
          uint32_t host_id = engine::get_host_id(hst->name());
          if (host_id != 0) {
            auto old_cvar{std::make_shared<custom_variable>()};
            old_cvar->enabled = false;
            old_cvar->host_id = host_id;
            old_cvar->name = misc::string::check_string_utf8(cvar->var_name);
            old_cvar->var_type = 0;
            old_cvar->update_time = cvar->timestamp.tv_sec;

            // Send custom variable event.
            log_v2::neb()->info(
                "callbacks: deleted custom variable '{}' on host {}",
                old_cvar->name, old_cvar->host_id);
            neb::gl_publisher.write(old_cvar);
          }
        }
      }
      // Service custom variable.
      else if (NEBTYPE_SERVICECUSTOMVARIABLE_ADD == cvar->type) {
        engine::service* svc{static_cast<engine::service*>(cvar->object_ptr)};
        if (svc && !svc->get_description().empty() &&
            !svc->get_hostname().empty()) {
          // Fill custom variable event.
          std::pair<uint32_t, uint32_t> p;
          p = engine::get_host_and_service_id(svc->get_hostname(),
                                              svc->get_description());
          if (p.first && p.second) {
            auto new_cvar{std::make_shared<custom_variable>()};
            new_cvar->enabled = true;
            new_cvar->host_id = p.first;
            new_cvar->modified = false;
            new_cvar->name = misc::string::check_string_utf8(cvar->var_name);
            new_cvar->service_id = p.second;
            new_cvar->var_type = 1;
            new_cvar->update_time = cvar->timestamp.tv_sec;
            new_cvar->value = misc::string::check_string_utf8(cvar->var_value);
            new_cvar->default_value =
                misc::string::check_string_utf8(cvar->var_value);

            // Send custom variable event.
            log_v2::neb()->info(
                "callbacks: new custom variable '{}' on service ({}, {})",
                new_cvar->name, new_cvar->host_id, new_cvar->service_id);
            neb::gl_publisher.write(new_cvar);
          }
        }
      } else if (NEBTYPE_SERVICECUSTOMVARIABLE_DELETE == cvar->type) {
        engine::service* svc{static_cast<engine::service*>(cvar->object_ptr)};
        if (svc && !svc->get_description().empty() &&
            !svc->get_hostname().empty()) {
          std::pair<uint64_t, uint64_t> p{engine::get_host_and_service_id(
              svc->get_hostname(), svc->get_description())};
          if (p.first && p.second) {
            auto old_cvar{std::make_shared<custom_variable>()};
            old_cvar->enabled = false;
            old_cvar->host_id = p.first;
            old_cvar->modified = true;
            old_cvar->name = misc::string::check_string_utf8(cvar->var_name);
            old_cvar->service_id = p.second;
            old_cvar->var_type = 1;
            old_cvar->update_time = cvar->timestamp.tv_sec;

            // Send custom variable event.
            log_v2::neb()->info(
                "callbacks: deleted custom variable '{}' on service ({},{})",
                old_cvar->name, old_cvar->host_id, old_cvar->service_id);
            neb::gl_publisher.write(old_cvar);
          }
        }
      }
    }
  }
  // Avoid exception propagation to C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process dependency data.
 *
 *  This function is called by Centreon Engine when some dependency data
 *  is available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_ADAPTIVE_DEPENDENCY_DATA).
 *  @param[in] data          A pointer to a
 *                           nebstruct_adaptive_dependency_data
 *                           containing the dependency data.
 *
 *  @return 0 on success.
 */
int neb::callback_dependency(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating dependency event");
  (void)callback_type;

  try {
    // Input variables.
    nebstruct_adaptive_dependency_data* nsadd(
        static_cast<nebstruct_adaptive_dependency_data*>(data));

    // Host dependency.
    if ((NEBTYPE_HOSTDEPENDENCY_ADD == nsadd->type) ||
        (NEBTYPE_HOSTDEPENDENCY_UPDATE == nsadd->type) ||
        (NEBTYPE_HOSTDEPENDENCY_DELETE == nsadd->type)) {
      // Find IDs.
      uint64_t host_id;
      uint64_t dep_host_id;
      engine::hostdependency* dep(
          static_cast<engine::hostdependency*>(nsadd->object_ptr));
      if (!dep->get_hostname().empty()) {
        host_id = engine::get_host_id(dep->get_hostname());
      } else {
        log_v2::neb()->error(
            "callbacks: dependency callback called without valid host");
        host_id = 0;
      }
      if (!dep->get_dependent_hostname().empty()) {
        dep_host_id = engine::get_host_id(dep->get_dependent_hostname());
      } else {
        log_v2::neb()->info(
            "callbacks: dependency callback called without valid dependent "
            "host");
        dep_host_id = 0;
      }

      // Generate service dependency event.
      auto hst_dep{std::make_shared<host_dependency>()};
      hst_dep->host_id = host_id;
      hst_dep->dependent_host_id = dep_host_id;
      hst_dep->enabled = (nsadd->type != NEBTYPE_HOSTDEPENDENCY_DELETE);
      if (!dep->get_dependency_period().empty())
        hst_dep->dependency_period = dep->get_dependency_period();
      {
        std::string options;
        if (dep->get_fail_on_down())
          options.append("d");
        if (dep->get_fail_on_up())
          options.append("o");
        if (dep->get_fail_on_pending())
          options.append("p");
        if (dep->get_fail_on_unreachable())
          options.append("u");
        if (dep->get_dependency_type() == engine::dependency::notification)
          hst_dep->notification_failure_options = options;
        else if (dep->get_dependency_type() == engine::dependency::execution)
          hst_dep->execution_failure_options = options;
      }
      hst_dep->inherits_parent = dep->get_inherits_parent();
      log_v2::neb()->info("callbacks: host {} depends on host {}", dep_host_id,
                          host_id);

      // Publish dependency event.
      neb::gl_publisher.write(hst_dep);
    }
    // Service dependency.
    else if ((NEBTYPE_SERVICEDEPENDENCY_ADD == nsadd->type) ||
             (NEBTYPE_SERVICEDEPENDENCY_UPDATE == nsadd->type) ||
             (NEBTYPE_SERVICEDEPENDENCY_DELETE == nsadd->type)) {
      // Find IDs.
      std::pair<uint64_t, uint64_t> ids;
      std::pair<uint64_t, uint64_t> dep_ids;
      engine::servicedependency* dep(
          static_cast<engine::servicedependency*>(nsadd->object_ptr));
      if (!dep->get_hostname().empty() &&
          !dep->get_service_description().empty()) {
        ids = engine::get_host_and_service_id(dep->get_hostname(),
                                              dep->get_service_description());
      } else {
        log_v2::neb()->error(
            "callbacks: dependency callback called without valid service");
        ids.first = 0;
        ids.second = 0;
      }
      if (!dep->get_dependent_hostname().empty() &&
          !dep->get_dependent_service_description().empty()) {
        dep_ids = engine::get_host_and_service_id(
            dep->get_hostname(), dep->get_service_description());
      } else {
        log_v2::neb()->error(
            "callbacks: dependency callback called without valid dependent "
            "service");
        dep_ids.first = 0;
        dep_ids.second = 0;
      }

      // Generate service dependency event.
      auto svc_dep{std::make_shared<service_dependency>()};
      svc_dep->host_id = ids.first;
      svc_dep->service_id = ids.second;
      svc_dep->dependent_host_id = dep_ids.first;
      svc_dep->dependent_service_id = dep_ids.second;
      svc_dep->enabled = (nsadd->type != NEBTYPE_SERVICEDEPENDENCY_DELETE);
      if (!dep->get_dependency_period().empty())
        svc_dep->dependency_period = dep->get_dependency_period();
      {
        std::string options;
        if (dep->get_fail_on_critical())
          options.append("c");
        if (dep->get_fail_on_ok())
          options.append("o");
        if (dep->get_fail_on_pending())
          options.append("p");
        if (dep->get_fail_on_unknown())
          options.append("u");
        if (dep->get_fail_on_warning())
          options.append("w");
        if (dep->get_dependency_type() == engine::dependency::notification)
          svc_dep->notification_failure_options = options;
        else if (dep->get_dependency_type() == engine::dependency::execution)
          svc_dep->execution_failure_options = options;
      }
      svc_dep->inherits_parent = dep->get_inherits_parent();
      log_v2::neb()->info(
          "callbacks: service ({}, {}) depends on service ({}, {})",
          dep_ids.first, dep_ids.second, ids.first, ids.second);

      // Publish dependency event.
      neb::gl_publisher.write(svc_dep);
    }
  }
  // Avoid exception propagation to C code.
  catch (...) {
  }

  return 0;
}

/**
 *  @brief Function that process downtime data.
 *
 *  This function is called by Nagios when some downtime data are available.
 *
 *  @param[in] callback_type Type of the callback (NEBCALLBACK_DOWNTIME_DATA).
 *  @param[in] data          A pointer to a nebstruct_downtime_data containing
 *                           the downtime data.
 *
 *  @return 0 on success.
 */
int neb::callback_downtime(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating downtime event");
  (void)callback_type;
  nebstruct_downtime_data const* downtime_data{
      static_cast<nebstruct_downtime_data*>(data)};
  if (downtime_data->type == NEBTYPE_DOWNTIME_LOAD)
    return 0;

  try {
    // In/Out variables.
    std::shared_ptr<neb::downtime> downtime(std::make_shared<neb::downtime>());

    // Fill output var.
    if (downtime_data->author_name)
      downtime->author =
          misc::string::check_string_utf8(downtime_data->author_name);
    if (downtime_data->comment_data)
      downtime->comment =
          misc::string::check_string_utf8(downtime_data->comment_data);
    downtime->downtime_type = downtime_data->downtime_type;
    downtime->duration = downtime_data->duration;
    downtime->end_time = downtime_data->end_time;
    downtime->entry_time = downtime_data->entry_time;
    downtime->fixed = downtime_data->fixed;
    if (!downtime_data->host_name)
      throw msg_fmt("unnamed host");
    if (downtime_data->service_description) {
      std::pair<uint32_t, uint32_t> p;
      p = engine::get_host_and_service_id(downtime_data->host_name,
                                          downtime_data->service_description);
      downtime->host_id = p.first;
      downtime->service_id = p.second;
      if (!downtime->host_id || !downtime->service_id)
        throw msg_fmt("could not find ID of service ('{}', '{}')",
                      downtime_data->host_name,
                      downtime_data->service_description);

    } else {
      downtime->host_id = engine::get_host_id(downtime_data->host_name);
      if (downtime->host_id == 0)
        throw msg_fmt("could not find ID of host '{}'",
                      downtime_data->host_name);
    }
    downtime->poller_id = config::applier::state::instance().poller_id();
    downtime->internal_id = downtime_data->downtime_id;
    downtime->start_time = downtime_data->start_time;
    downtime->triggered_by = downtime_data->triggered_by;
    private_downtime_params& params(downtimes[downtime->internal_id]);
    switch (downtime_data->type) {
      case NEBTYPE_DOWNTIME_ADD:
        params.cancelled = false;
        params.deletion_time = -1;
        params.end_time = -1;
        params.started = false;
        params.start_time = -1;
        break;
      case NEBTYPE_DOWNTIME_START:
        params.started = true;
        params.start_time = downtime_data->timestamp.tv_sec;
        break;
      case NEBTYPE_DOWNTIME_STOP:
        if (NEBATTR_DOWNTIME_STOP_CANCELLED == downtime_data->attr)
          params.cancelled = true;
        params.end_time = downtime_data->timestamp.tv_sec;
        break;
      case NEBTYPE_DOWNTIME_DELETE:
        if (!params.started)
          params.cancelled = true;
        params.deletion_time = downtime_data->timestamp.tv_sec;
        break;
      default:
        throw msg_fmt("Downtime with not managed type {}.",
                      downtime_data->host_name);
    }
    downtime->actual_start_time = params.start_time;
    downtime->actual_end_time = params.end_time;
    downtime->deletion_time = params.deletion_time;
    downtime->was_cancelled = params.cancelled;
    downtime->was_started = params.started;
    if (NEBTYPE_DOWNTIME_DELETE == downtime_data->type)
      downtimes.erase(downtime->internal_id);

    // Send event.
    gl_publisher.write(downtime);
  } catch (std::exception const& e) {
    log_v2::neb()->error(
        "callbacks: error occurred while generating downtime event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process event handler data.
 *
 *  This function is called by Nagios when some event handler data are
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_EVENT_HANDLER_DATA).
 *  @param[in] data          A pointer to a nebstruct_event_handler_data
 *                           containing the event handler data.
 *
 *  @return 0 on success.
 */
int neb::callback_event_handler(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating event handler event");
  (void)callback_type;

  try {
    // In/Out variables.
    nebstruct_event_handler_data const* event_handler_data;
    auto event_handler = std::make_shared<neb::event_handler>();

    // Fill output var.
    event_handler_data = static_cast<nebstruct_event_handler_data*>(data);
    if (!event_handler_data->command_args.empty())
      event_handler->command_args =
          misc::string::check_string_utf8(event_handler_data->command_args);
    if (event_handler_data->command_line)
      event_handler->command_line =
          misc::string::check_string_utf8(event_handler_data->command_line);
    event_handler->early_timeout = event_handler_data->early_timeout;
    event_handler->end_time = event_handler_data->end_time.tv_sec;
    event_handler->execution_time = event_handler_data->execution_time;
    if (!event_handler_data->host_name)
      throw msg_fmt("unnamed host");
    if (event_handler_data->service_description) {
      std::pair<uint32_t, uint32_t> p;
      p = engine::get_host_and_service_id(
          event_handler_data->host_name,
          event_handler_data->service_description);
      event_handler->host_id = p.first;
      event_handler->service_id = p.second;
      if (!event_handler->host_id || !event_handler->service_id)
        throw msg_fmt("could not find ID of service ('{}', '{}')",
                      event_handler_data->host_name,
                      event_handler_data->service_description);
    } else {
      event_handler->host_id =
          engine::get_host_id(event_handler_data->host_name);
      if (event_handler->host_id == 0)
        throw msg_fmt("could not find ID of host '{}'",
                      event_handler_data->host_name);
    }
    if (event_handler_data->output)
      event_handler->output =
          misc::string::check_string_utf8(event_handler_data->output);
    event_handler->return_code = event_handler_data->return_code;
    event_handler->start_time = event_handler_data->start_time.tv_sec;
    event_handler->state = event_handler_data->state;
    event_handler->state_type = event_handler_data->state_type;
    event_handler->timeout = event_handler_data->timeout;
    event_handler->handler_type = event_handler_data->eventhandler_type;

    // Send event.
    gl_publisher.write(event_handler);
  } catch (std::exception const& e) {
    log_v2::neb()->error(
        "callbacks: error occurred while generating event handler event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process external commands.
 *
 *  This function is called by the monitoring engine when some external
 *  command is received.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_EXTERNALCOMMAND_DATA).
 *  @param[in] data          A pointer to a nebstruct_externalcommand_data
 *                           containing the external command data.
 *
 *  @return 0 on success.
 */
int neb::callback_external_command(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->debug("callbacks: external command data");
  (void)callback_type;

  nebstruct_external_command_data* necd(
      static_cast<nebstruct_external_command_data*>(data));
  if (necd && (necd->type == NEBTYPE_EXTERNALCOMMAND_START)) {
    try {
      if (necd->command_type == CMD_CHANGE_CUSTOM_HOST_VAR) {
        log_v2::neb()->info(
            "callbacks: generating host custom variable update event");

        // Split argument string.
        if (necd->command_args) {
          std::list<std::string> l{absl::StrSplit(
              misc::string::check_string_utf8(necd->command_args), ';')};
          if (l.size() != 3)
            log_v2::neb()->error(
                "callbacks: invalid host custom variable command");
          else {
            std::list<std::string>::iterator it(l.begin());
            std::string host{std::move(*it)};
            ++it;
            std::string var_name{std::move(*it)};
            ++it;
            std::string var_value{std::move(*it)};

            // Find host ID.
            uint64_t host_id = engine::get_host_id(host);
            if (host_id != 0) {
              // Fill custom variable.
              auto cvs = std::make_shared<neb::custom_variable_status>();
              cvs->host_id = host_id;
              cvs->modified = true;
              cvs->name = var_name;
              cvs->service_id = 0;
              cvs->update_time = necd->timestamp.tv_sec;
              cvs->value = var_value;

              // Send event.
              gl_publisher.write(cvs);
            }
          }
        }
      } else if (necd->command_type == CMD_CHANGE_CUSTOM_SVC_VAR) {
        log_v2::neb()->info(
            "callbacks: generating service custom variable update event");

        // Split argument string.
        if (necd->command_args) {
          std::list<std::string> l{absl::StrSplit(
              misc::string::check_string_utf8(necd->command_args), ';')};
          if (l.size() != 4)
            log_v2::neb()->error(
                "callbacks: invalid service custom variable command");
          else {
            std::list<std::string>::iterator it{l.begin()};
            std::string host{std::move(*it)};
            ++it;
            std::string service{std::move(*it)};
            ++it;
            std::string var_name{std::move(*it)};
            ++it;
            std::string var_value{std::move(*it)};

            // Find host/service IDs.
            std::pair<uint64_t, uint64_t> p{
                engine::get_host_and_service_id(host, service)};
            if (p.first && p.second) {
              // Fill custom variable.
              auto cvs{std::make_shared<neb::custom_variable_status>()};
              cvs->host_id = p.first;
              cvs->modified = true;
              cvs->name = var_name;
              cvs->service_id = p.second;
              cvs->update_time = necd->timestamp.tv_sec;
              cvs->value = var_value;

              // Send event.
              gl_publisher.write(cvs);
            }
          }
        }
      }
    }
    // Avoid exception propagation in C code.
    catch (...) {
    }
  }
  return 0;
}

/**
 *  @brief Function that process flapping status data.
 *
 *  This function is called by Nagios when some flapping status data is
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_FLAPPING_DATA).
 *  @param[in] data          A pointer to a nebstruct_flapping_data
 *                           containing the flapping status data.
 *
 *  @return 0 on success.
 */
int neb::callback_flapping_status(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating flapping event");
  (void)callback_type;

  try {
    // In/Out variables.
    nebstruct_flapping_data const* flapping_data;
    auto flapping_status{std::make_shared<neb::flapping_status>()};

    // Fill output var.
    flapping_data = static_cast<nebstruct_flapping_data*>(data);
    flapping_status->event_time = flapping_data->timestamp.tv_sec;
    flapping_status->event_type = flapping_data->type;
    flapping_status->high_threshold = flapping_data->high_threshold;
    if (flapping_data->service_id == 0) {
      flapping_status->host_id = flapping_data->host_id;
    } else {
      flapping_status->host_id = flapping_data->host_id;
      flapping_status->service_id = flapping_data->service_id;
    }
    flapping_status->low_threshold = flapping_data->low_threshold;
    flapping_status->percent_state_change = flapping_data->percent_change;
    // flapping_status->reason_type = XXX;
    flapping_status->flapping_type = flapping_data->flapping_type;

    // Send event.
    gl_publisher.write(flapping_status);
  } catch (std::exception const& e) {
    log_v2::neb()->error(
        "callbacks: error occurred while generating flapping event: {}",
        e.what());
  }
  // Avoid exception propagation to C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process group data.
 *
 *  This function is called by Engine when some group data is available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_GROUP_DATA).
 *  @param[in] data          Pointer to a nebstruct_group_data
 *                           containing the group data.
 *
 *  @return 0 on success.
 */
int neb::callback_group(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating group event");
  (void)callback_type;

  try {
    // Input variable.
    nebstruct_group_data const* group_data(
        static_cast<nebstruct_group_data*>(data));

    // Host group.
    if ((NEBTYPE_HOSTGROUP_ADD == group_data->type) ||
        (NEBTYPE_HOSTGROUP_UPDATE == group_data->type) ||
        (NEBTYPE_HOSTGROUP_DELETE == group_data->type)) {
      engine::hostgroup const* host_group(
          static_cast<engine::hostgroup*>(group_data->object_ptr));
      if (!host_group->get_group_name().empty()) {
        auto new_hg{std::make_shared<neb::host_group>()};
        new_hg->poller_id = config::applier::state::instance().poller_id();
        new_hg->id = host_group->get_id();
        new_hg->enabled = (group_data->type != NEBTYPE_HOSTGROUP_DELETE &&
                           !host_group->members.empty());
        new_hg->name =
            misc::string::check_string_utf8(host_group->get_group_name());

        // Send host group event.
        if (new_hg->id) {
          log_v2::neb()->info(
              "callbacks: new host group {} ('{}') on instance {}", new_hg->id,
              new_hg->name, new_hg->poller_id);
          neb::gl_publisher.write(new_hg);
        }
      }
    }
    // Service group.
    else if ((NEBTYPE_SERVICEGROUP_ADD == group_data->type) ||
             (NEBTYPE_SERVICEGROUP_UPDATE == group_data->type) ||
             (NEBTYPE_SERVICEGROUP_DELETE == group_data->type)) {
      engine::servicegroup const* service_group(
          static_cast<engine::servicegroup*>(group_data->object_ptr));
      if (!service_group->get_group_name().empty()) {
        auto new_sg{std::make_shared<neb::service_group>()};
        new_sg->poller_id = config::applier::state::instance().poller_id();
        new_sg->id = service_group->get_id();
        new_sg->enabled = (group_data->type != NEBTYPE_SERVICEGROUP_DELETE &&
                           !service_group->members.empty());
        new_sg->name =
            misc::string::check_string_utf8(service_group->get_group_name());

        // Send service group event.
        if (new_sg->id) {
          log_v2::neb()->info(
              "callbacks:: new service group {} ('{}) on instance {}",
              new_sg->id, new_sg->name, new_sg->poller_id);
          neb::gl_publisher.write(new_sg);
        }
      }
    }
  }
  // Avoid exception propagation to C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process group membership.
 *
 *  This function is called by Engine when some group membership data is
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_GROUPMEMBER_DATA).
 *  @param[in] data          Pointer to a nebstruct_group_member_data
 *                           containing membership data.
 *
 *  @return 0 on success.
 */
int neb::callback_group_member(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating group member event");
  (void)callback_type;

  try {
    // Input variable.
    nebstruct_group_member_data const* member_data(
        static_cast<nebstruct_group_member_data*>(data));

    // Host group member.
    if ((member_data->type == NEBTYPE_HOSTGROUPMEMBER_ADD) ||
        (member_data->type == NEBTYPE_HOSTGROUPMEMBER_DELETE)) {
      engine::host const* hst(
          static_cast<engine::host*>(member_data->object_ptr));
      engine::hostgroup const* hg(
          static_cast<engine::hostgroup*>(member_data->group_ptr));
      if (!hst->name().empty() && !hg->get_group_name().empty()) {
        // Output variable.
        auto hgm{std::make_shared<neb::host_group_member>()};
        hgm->group_id = hg->get_id();
        hgm->group_name = misc::string::check_string_utf8(hg->get_group_name());
        hgm->poller_id = config::applier::state::instance().poller_id();
        uint32_t host_id = engine::get_host_id(hst->name());
        if (host_id != 0 && hgm->group_id != 0) {
          hgm->host_id = host_id;
          if (member_data->type == NEBTYPE_HOSTGROUPMEMBER_DELETE) {
            log_v2::neb()->info(
                "callbacks: host {} is not a member of group {} on instance {} "
                "anymore",
                hgm->host_id, hgm->group_id, hgm->poller_id);
            hgm->enabled = false;
          } else {
            log_v2::neb()->info(
                "callbacks: host {} is a member of group {} on instance {}",
                hgm->host_id, hgm->group_id, hgm->poller_id);
            hgm->enabled = true;
          }

          // Send host group member event.
          if (hgm->host_id && hgm->group_id)
            neb::gl_publisher.write(hgm);
        }
      }
    }
    // Service group member.
    else if ((member_data->type == NEBTYPE_SERVICEGROUPMEMBER_ADD) ||
             (member_data->type == NEBTYPE_SERVICEGROUPMEMBER_DELETE)) {
      engine::service const* svc(
          static_cast<engine::service*>(member_data->object_ptr));
      engine::servicegroup const* sg(
          static_cast<engine::servicegroup*>(member_data->group_ptr));
      if (!svc->get_description().empty() && !sg->get_group_name().empty() &&
          !svc->get_hostname().empty()) {
        // Output variable.
        auto sgm{std::make_shared<neb::service_group_member>()};
        sgm->group_id = sg->get_id();
        sgm->group_name = misc::string::check_string_utf8(sg->get_group_name());
        sgm->poller_id = config::applier::state::instance().poller_id();
        std::pair<uint32_t, uint32_t> p;
        p = engine::get_host_and_service_id(svc->get_hostname(),
                                            svc->get_description());
        sgm->host_id = p.first;
        sgm->service_id = p.second;
        if (sgm->host_id && sgm->service_id && sgm->group_id) {
          if (member_data->type == NEBTYPE_SERVICEGROUPMEMBER_DELETE) {
            log_v2::neb()->info(
                "callbacks: service ({},{}) is not a member of group {} on "
                "instance {} anymore",
                sgm->host_id, sgm->service_id, sgm->group_id, sgm->poller_id);
            sgm->enabled = false;
          } else {
            log_v2::neb()->info(
                "callbacks: service ({}, {}) is a member of group {} on "
                "instance {}",
                sgm->host_id, sgm->service_id, sgm->group_id, sgm->poller_id);
            sgm->enabled = true;
          }

          // Send service group member event.
          if (sgm->host_id && sgm->service_id && sgm->group_id)
            neb::gl_publisher.write(sgm);
        }
      }
    }
  }
  // Avoid exception propagation to C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process host data.
 *
 *  This function is called by Engine when some host data is available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_HOST_DATA).
 *  @param[in] data          A pointer to a nebstruct_host_data
 *                           containing a host data.
 *
 *  @return 0 on success.
 */
int neb::callback_host(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating host event");
  (void)callback_type;

  try {
    // In/Out variables.
    const nebstruct_adaptive_host_data* host_data(
        static_cast<nebstruct_adaptive_host_data*>(data));
    if (host_data->flags & NEBATTR_BBDO3_ONLY)
      return 0;
    const engine::host* h(static_cast<engine::host*>(host_data->object_ptr));
    auto my_host{std::make_shared<neb::host>()};

    // Set host parameters.
    my_host->acknowledged = h->problem_has_been_acknowledged();
    my_host->acknowledgement_type = h->get_acknowledgement();
    if (!h->get_action_url().empty())
      my_host->action_url =
          misc::string::check_string_utf8(h->get_action_url());
    my_host->active_checks_enabled = h->active_checks_enabled();
    if (!h->get_address().empty())
      my_host->address = misc::string::check_string_utf8(h->get_address());
    if (!h->get_alias().empty())
      my_host->alias = misc::string::check_string_utf8(h->get_alias());
    my_host->check_freshness = h->check_freshness_enabled();
    if (!h->check_command().empty())
      my_host->check_command =
          misc::string::check_string_utf8(h->check_command());
    my_host->check_interval = h->check_interval();
    if (!h->check_period().empty())
      my_host->check_period = h->check_period();
    my_host->check_type = h->get_check_type();
    my_host->current_check_attempt = h->get_current_attempt();
    my_host->current_state =
        (h->has_been_checked() ? h->get_current_state() : 4);  // Pending state.
    my_host->default_active_checks_enabled = h->active_checks_enabled();
    my_host->default_event_handler_enabled = h->event_handler_enabled();
    my_host->default_flap_detection_enabled = h->flap_detection_enabled();
    my_host->default_notifications_enabled = h->get_notifications_enabled();
    my_host->default_passive_checks_enabled = h->passive_checks_enabled();
    my_host->downtime_depth = h->get_scheduled_downtime_depth();
    if (!h->get_display_name().empty())
      my_host->display_name =
          misc::string::check_string_utf8(h->get_display_name());
    my_host->enabled = (host_data->type != NEBTYPE_HOST_DELETE);
    if (!h->event_handler().empty())
      my_host->event_handler =
          misc::string::check_string_utf8(h->event_handler());
    my_host->event_handler_enabled = h->event_handler_enabled();
    my_host->execution_time = h->get_execution_time();
    my_host->first_notification_delay = h->get_first_notification_delay();
    my_host->notification_number = h->get_notification_number();
    my_host->flap_detection_enabled = h->flap_detection_enabled();
    my_host->flap_detection_on_down =
        h->get_flap_detection_on(engine::notifier::down);
    my_host->flap_detection_on_unreachable =
        h->get_flap_detection_on(engine::notifier::unreachable);
    my_host->flap_detection_on_up =
        h->get_flap_detection_on(engine::notifier::up);
    my_host->freshness_threshold = h->get_freshness_threshold();
    my_host->has_been_checked = h->has_been_checked();
    my_host->high_flap_threshold = h->get_high_flap_threshold();
    if (!h->name().empty())
      my_host->host_name = misc::string::check_string_utf8(h->name());
    if (!h->get_icon_image().empty())
      my_host->icon_image =
          misc::string::check_string_utf8(h->get_icon_image());
    if (!h->get_icon_image_alt().empty())
      my_host->icon_image_alt =
          misc::string::check_string_utf8(h->get_icon_image_alt());
    my_host->is_flapping = h->get_is_flapping();
    my_host->last_check = h->get_last_check();
    my_host->last_hard_state = h->get_last_hard_state();
    my_host->last_hard_state_change = h->get_last_hard_state_change();
    my_host->last_notification = h->get_last_notification();
    my_host->last_state_change = h->get_last_state_change();
    my_host->last_time_down = h->get_last_time_down();
    my_host->last_time_unreachable = h->get_last_time_unreachable();
    my_host->last_time_up = h->get_last_time_up();
    my_host->last_update = time(nullptr);
    my_host->latency = h->get_latency();
    my_host->low_flap_threshold = h->get_low_flap_threshold();
    my_host->max_check_attempts = h->max_check_attempts();
    my_host->next_check = h->get_next_check();
    my_host->next_notification = h->get_next_notification();
    my_host->no_more_notifications = h->get_no_more_notifications();
    if (!h->get_notes().empty())
      my_host->notes = misc::string::check_string_utf8(h->get_notes());
    if (!h->get_notes_url().empty())
      my_host->notes_url = misc::string::check_string_utf8(h->get_notes_url());
    my_host->notifications_enabled = h->get_notifications_enabled();
    my_host->notification_interval = h->get_notification_interval();
    if (!h->notification_period().empty())
      my_host->notification_period = h->notification_period();
    my_host->notify_on_down = h->get_notify_on(engine::notifier::down);
    my_host->notify_on_downtime = h->get_notify_on(engine::notifier::downtime);
    my_host->notify_on_flapping =
        h->get_notify_on(engine::notifier::flappingstart);
    my_host->notify_on_recovery = h->get_notify_on(engine::notifier::up);
    my_host->notify_on_unreachable =
        h->get_notify_on(engine::notifier::unreachable);
    my_host->obsess_over = h->obsess_over();
    if (!h->get_plugin_output().empty()) {
      my_host->output = misc::string::check_string_utf8(h->get_plugin_output());
      my_host->output.append("\n");
    }
    if (!h->get_long_plugin_output().empty())
      my_host->output.append(
          misc::string::check_string_utf8(h->get_long_plugin_output()));
    my_host->passive_checks_enabled = h->passive_checks_enabled();
    my_host->percent_state_change = h->get_percent_state_change();
    if (!h->get_perf_data().empty())
      my_host->perf_data = misc::string::check_string_utf8(h->get_perf_data());
    my_host->poller_id = config::applier::state::instance().poller_id();
    my_host->retain_nonstatus_information =
        h->get_retain_nonstatus_information();
    my_host->retain_status_information = h->get_retain_status_information();
    my_host->retry_interval = h->retry_interval();
    my_host->should_be_scheduled = h->get_should_be_scheduled();
    my_host->stalk_on_down = h->get_stalk_on(engine::notifier::down);
    my_host->stalk_on_unreachable =
        h->get_stalk_on(engine::notifier::unreachable);
    my_host->stalk_on_up = h->get_stalk_on(engine::notifier::up);
    my_host->state_type =
        (h->has_been_checked() ? h->get_state_type() : engine::notifier::hard);
    if (!h->get_statusmap_image().empty())
      my_host->statusmap_image =
          misc::string::check_string_utf8(h->get_statusmap_image());
    my_host->timezone = h->get_timezone();

    // Find host ID.
    uint64_t host_id = engine::get_host_id(my_host->host_name);
    if (host_id != 0) {
      my_host->host_id = host_id;

      // Send host event.
      log_v2::neb()->info("callbacks:  new host {} ('{}') on instance {}",
                          my_host->host_id, my_host->host_name,
                          my_host->poller_id);
      neb::gl_publisher.write(my_host);

      /* No need to send this service custom variables changes, custom variables
       * are managed in a different loop. */
    } else
      log_v2::neb()->error("callbacks: host '{}' has no ID (yet) defined",
                           (!h->name().empty() ? h->name() : "(unknown)"));
  }
  // Avoid exception propagation to C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process host protobuf data.
 *
 *  This function is called by Engine when some host data is available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_HOST_DATA).
 *  @param[in] data          A pointer to a nebstruct_host_data
 *                           containing a host data.
 *
 *  @return 0 on success.
 */
int neb::callback_pb_host(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating pb host event protobuf");
  (void)callback_type;

  nebstruct_adaptive_host_data* dh =
      static_cast<nebstruct_adaptive_host_data*>(data);
  const engine::host* eh{static_cast<engine::host*>(dh->object_ptr)};

  if (dh->type == NEBTYPE_ADAPTIVEHOST_UPDATE &&
      dh->modified_attribute != MODATTR_ALL) {
    auto h{std::make_shared<neb::pb_adaptive_host>()};
    AdaptiveHost& hst = h.get()->mut_obj();
    if (dh->modified_attribute & MODATTR_NOTIFICATIONS_ENABLED)
      hst.set_notify(eh->get_notifications_enabled());
    else if (dh->modified_attribute & MODATTR_ACTIVE_CHECKS_ENABLED) {
      hst.set_active_checks(eh->active_checks_enabled());
      hst.set_should_be_scheduled(eh->get_should_be_scheduled());
    } else if (dh->modified_attribute & MODATTR_PASSIVE_CHECKS_ENABLED)
      hst.set_passive_checks(eh->passive_checks_enabled());
    else if (dh->modified_attribute & MODATTR_EVENT_HANDLER_ENABLED)
      hst.set_event_handler_enabled(eh->event_handler_enabled());
    else if (dh->modified_attribute & MODATTR_FLAP_DETECTION_ENABLED)
      hst.set_flap_detection(eh->flap_detection_enabled());
    else if (dh->modified_attribute & MODATTR_OBSESSIVE_HANDLER_ENABLED)
      hst.set_obsess_over_host(eh->obsess_over());
    else if (dh->modified_attribute & MODATTR_EVENT_HANDLER_COMMAND)
      hst.set_event_handler(
          misc::string::check_string_utf8(eh->event_handler()));
    else if (dh->modified_attribute & MODATTR_CHECK_COMMAND)
      hst.set_check_command(
          misc::string::check_string_utf8(eh->check_command()));
    else if (dh->modified_attribute & MODATTR_NORMAL_CHECK_INTERVAL)
      hst.set_check_interval(eh->check_interval());
    else if (dh->modified_attribute & MODATTR_RETRY_CHECK_INTERVAL)
      hst.set_retry_interval(eh->retry_interval());
    else if (dh->modified_attribute & MODATTR_MAX_CHECK_ATTEMPTS)
      hst.set_max_check_attempts(eh->max_check_attempts());
    else if (dh->modified_attribute & MODATTR_FRESHNESS_CHECKS_ENABLED)
      hst.set_check_freshness(eh->check_freshness_enabled());
    else if (dh->modified_attribute & MODATTR_CHECK_TIMEPERIOD)
      hst.set_check_period(eh->check_period());
    else if (dh->modified_attribute & MODATTR_NOTIFICATION_TIMEPERIOD)
      hst.set_notification_period(eh->notification_period());
    else {
      log_v2::neb()->error("callbacks: adaptive host not implemented.");
      assert(1 == 0);
    }

    uint64_t host_id = engine::get_host_id(eh->name());
    if (host_id != 0) {
      hst.set_host_id(host_id);

      // Send host event.
      log_v2::neb()->info("callbacks:  new host {} ('{}') on instance {}",
                          hst.host_id(), eh->name(),
                          config::applier::state::instance().poller_id());
      neb::gl_publisher.write(h);
    } else
      log_v2::neb()->error("callbacks: host '{}' has no ID (yet) defined",
                           (!eh->name().empty() ? eh->name() : "(unknown)"));
  } else {
    auto h{std::make_shared<neb::pb_host>()};
    Host& host = h.get()->mut_obj();

    // Set host parameters.
    host.set_acknowledged(eh->problem_has_been_acknowledged());
    host.set_acknowledgement_type(eh->get_acknowledgement());
    if (!eh->get_action_url().empty())
      host.set_action_url(
          misc::string::check_string_utf8(eh->get_action_url()));
    host.set_active_checks(eh->active_checks_enabled());
    if (!eh->get_address().empty())
      host.set_address(misc::string::check_string_utf8(eh->get_address()));
    if (!eh->get_alias().empty())
      host.set_alias(misc::string::check_string_utf8(eh->get_alias()));
    host.set_check_freshness(eh->check_freshness_enabled());
    if (!eh->check_command().empty())
      host.set_check_command(
          misc::string::check_string_utf8(eh->check_command()));
    host.set_check_interval(eh->check_interval());
    if (!eh->check_period().empty())
      host.set_check_period(eh->check_period());
    host.set_check_type(static_cast<Host_CheckType>(eh->get_check_type()));
    host.set_check_attempt(eh->get_current_attempt());
    host.set_state(static_cast<Host_State>(eh->has_been_checked()
                                               ? eh->get_current_state()
                                               : 4));  // Pending state.
    host.set_default_active_checks(eh->active_checks_enabled());
    host.set_default_event_handler_enabled(eh->event_handler_enabled());
    host.set_default_flap_detection(eh->flap_detection_enabled());
    host.set_default_notify(eh->get_notifications_enabled());
    host.set_default_passive_checks(eh->passive_checks_enabled());
    host.set_scheduled_downtime_depth(eh->get_scheduled_downtime_depth());
    if (!eh->get_display_name().empty())
      host.set_display_name(
          misc::string::check_string_utf8(eh->get_display_name()));
    host.set_enabled(static_cast<nebstruct_host_status_data*>(data)->type !=
                     NEBTYPE_HOST_DELETE);
    if (!eh->event_handler().empty())
      host.set_event_handler(
          misc::string::check_string_utf8(eh->event_handler()));
    host.set_event_handler_enabled(eh->event_handler_enabled());
    host.set_execution_time(eh->get_execution_time());
    host.set_first_notification_delay(eh->get_first_notification_delay());
    host.set_notification_number(eh->get_notification_number());
    host.set_flap_detection(eh->flap_detection_enabled());
    host.set_flap_detection_on_down(
        eh->get_flap_detection_on(engine::notifier::down));
    host.set_flap_detection_on_unreachable(
        eh->get_flap_detection_on(engine::notifier::unreachable));
    host.set_flap_detection_on_up(
        eh->get_flap_detection_on(engine::notifier::up));
    host.set_freshness_threshold(eh->get_freshness_threshold());
    host.set_checked(eh->has_been_checked());
    host.set_high_flap_threshold(eh->get_high_flap_threshold());
    if (!eh->name().empty())
      host.set_name(misc::string::check_string_utf8(eh->name()));
    if (!eh->get_icon_image().empty())
      host.set_icon_image(
          misc::string::check_string_utf8(eh->get_icon_image()));
    if (!eh->get_icon_image_alt().empty())
      host.set_icon_image_alt(
          misc::string::check_string_utf8(eh->get_icon_image_alt()));
    host.set_flapping(eh->get_is_flapping());
    host.set_last_check(eh->get_last_check());
    host.set_last_hard_state(
        static_cast<Host_State>(eh->get_last_hard_state()));
    host.set_last_hard_state_change(eh->get_last_hard_state_change());
    host.set_last_notification(eh->get_last_notification());
    host.set_last_state_change(eh->get_last_state_change());
    host.set_last_time_down(eh->get_last_time_down());
    host.set_last_time_unreachable(eh->get_last_time_unreachable());
    host.set_last_time_up(eh->get_last_time_up());
    host.set_last_update(time(nullptr));
    host.set_latency(eh->get_latency());
    host.set_low_flap_threshold(eh->get_low_flap_threshold());
    host.set_max_check_attempts(eh->max_check_attempts());
    host.set_next_check(eh->get_next_check());
    host.set_next_host_notification(eh->get_next_notification());
    host.set_no_more_notifications(eh->get_no_more_notifications());
    if (!eh->get_notes().empty())
      host.set_notes(misc::string::check_string_utf8(eh->get_notes()));
    if (!eh->get_notes_url().empty())
      host.set_notes_url(misc::string::check_string_utf8(eh->get_notes_url()));
    host.set_notify(eh->get_notifications_enabled());
    host.set_notification_interval(eh->get_notification_interval());
    if (!eh->notification_period().empty())
      host.set_notification_period(eh->notification_period());
    host.set_notify_on_down(eh->get_notify_on(engine::notifier::down));
    host.set_notify_on_downtime(eh->get_notify_on(engine::notifier::downtime));
    host.set_notify_on_flapping(
        eh->get_notify_on(engine::notifier::flappingstart));
    host.set_notify_on_recovery(eh->get_notify_on(engine::notifier::up));
    host.set_notify_on_unreachable(
        eh->get_notify_on(engine::notifier::unreachable));
    host.set_obsess_over_host(eh->obsess_over());
    if (!eh->get_plugin_output().empty()) {
      host.set_output(misc::string::check_string_utf8(eh->get_plugin_output()));
    }
    if (!eh->get_long_plugin_output().empty())
      host.set_output(
          misc::string::check_string_utf8(eh->get_long_plugin_output()));
    host.set_passive_checks(eh->passive_checks_enabled());
    host.set_percent_state_change(eh->get_percent_state_change());
    if (!eh->get_perf_data().empty())
      host.set_perfdata(misc::string::check_string_utf8(eh->get_perf_data()));
    host.set_instance_id(config::applier::state::instance().poller_id());
    host.set_retain_nonstatus_information(
        eh->get_retain_nonstatus_information());
    host.set_retain_status_information(eh->get_retain_status_information());
    host.set_retry_interval(eh->retry_interval());
    host.set_should_be_scheduled(eh->get_should_be_scheduled());
    host.set_stalk_on_down(eh->get_stalk_on(engine::notifier::down));
    host.set_stalk_on_unreachable(
        eh->get_stalk_on(engine::notifier::unreachable));
    host.set_stalk_on_up(eh->get_stalk_on(engine::notifier::up));
    host.set_state_type(static_cast<Host_StateType>(
        eh->has_been_checked() ? eh->get_state_type()
                               : engine::notifier::hard));
    if (!eh->get_statusmap_image().empty())
      host.set_statusmap_image(
          misc::string::check_string_utf8(eh->get_statusmap_image()));
    host.set_timezone(eh->get_timezone());
    host.set_severity_id(eh->get_severity() ? eh->get_severity()->id() : 0);
    host.set_icon_id(eh->get_icon_id());
    for (auto& tg : eh->tags()) {
      TagInfo* ti = host.mutable_tags()->Add();
      ti->set_id(tg->id());
      ti->set_type(static_cast<TagType>(tg->type()));
    }

    // Find host ID.
    uint64_t host_id = engine::get_host_id(host.name());
    if (host_id != 0) {
      host.set_host_id(host_id);

      // Send host event.
      log_v2::neb()->info("callbacks:  new host {} ('{}') on instance {}",
                          host.host_id(), host.name(), host.instance_id());
      neb::gl_publisher.write(h);

      /* No need to send this service custom variables changes, custom variables
       * are managed in a different loop. */
    } else
      log_v2::neb()->error("callbacks: host '{}' has no ID (yet) defined",
                           (!eh->name().empty() ? eh->name() : "(unknown)"));
  }
  return 0;
}

/**
 *  @brief Function that process host check data.
 *
 *  This function is called by Nagios when some host check data are available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_HOST_CHECK_DATA).
 *  @param[in] data          A pointer to a nebstruct_host_check_data
 *                           containing the host check data.
 *
 *  @return 0 on success.
 */
int neb::callback_host_check(int callback_type, void* data) {
  (void)callback_type;

  // In/Out variables.
  nebstruct_host_check_data const* hcdata =
      static_cast<nebstruct_host_check_data*>(data);

  /* For each check, this event is received three times one precheck, one
   * initiate and one processed. We just keep the initiate one. At the processed
   * one we also received the host status. */
  if (hcdata->type != NEBTYPE_HOSTCHECK_INITIATE)
    return 0;

  // Log message.
  log_v2::neb()->info("callbacks: generating host check event");

  try {
    auto host_check{std::make_shared<neb::host_check>()};

    // Fill output var.
    engine::host* h(static_cast<engine::host*>(hcdata->object_ptr));
    if (hcdata->command_line) {
      host_check->active_checks_enabled = h->active_checks_enabled();
      host_check->check_type = hcdata->check_type;
      host_check->command_line =
          misc::string::check_string_utf8(hcdata->command_line);
      if (!hcdata->host_name)
        throw msg_fmt("unnamed host");
      host_check->host_id = engine::get_host_id(hcdata->host_name);
      if (host_check->host_id == 0)
        throw msg_fmt("could not find ID of host '{}'", hcdata->host_name);
      host_check->next_check = h->get_next_check();

      // Send event.
      gl_publisher.write(host_check);
    }
  } catch (std::exception const& e) {
    log_v2::neb()->error(
        "callbacks: error occurred while generating host check event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process host status data.
 *
 *  This function is called by Nagios when some host status data are available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_HOST_STATUS_DATA).
 *  @param[in] data          A pointer to a nebstruct_host_status_data
 *                           containing the host status data.
 *
 *  @return 0 on success.
 */
int neb::callback_host_status(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating host status event");
  (void)callback_type;

  try {
    // In/Out variables.
    auto host_status{std::make_shared<neb::host_status>()};

    // Fill output var.
    const engine::host* h = static_cast<engine::host*>(
        static_cast<nebstruct_host_status_data*>(data)->object_ptr);
    host_status->acknowledged = h->problem_has_been_acknowledged();
    host_status->acknowledgement_type = h->get_acknowledgement();
    host_status->active_checks_enabled = h->active_checks_enabled();
    if (!h->check_command().empty())
      host_status->check_command =
          misc::string::check_string_utf8(h->check_command());
    host_status->check_interval = h->check_interval();
    if (!h->check_period().empty())
      host_status->check_period = h->check_period();
    host_status->check_type = h->get_check_type();
    host_status->current_check_attempt = h->get_current_attempt();
    host_status->current_state =
        (h->has_been_checked() ? h->get_current_state() : 4);  // Pending state.
    host_status->downtime_depth = h->get_scheduled_downtime_depth();
    if (!h->event_handler().empty())
      host_status->event_handler =
          misc::string::check_string_utf8(h->event_handler());
    host_status->event_handler_enabled = h->event_handler_enabled();
    host_status->execution_time = h->get_execution_time();
    host_status->flap_detection_enabled = h->flap_detection_enabled();
    host_status->has_been_checked = h->has_been_checked();
    if (h->name().empty())
      throw msg_fmt("unnamed host");
    {
      host_status->host_id = engine::get_host_id(h->name());
      if (host_status->host_id == 0)
        throw msg_fmt("could not find ID of host '{}'", h->name());
    }
    host_status->is_flapping = h->get_is_flapping();
    host_status->last_check = h->get_last_check();
    host_status->last_hard_state = h->get_last_hard_state();
    host_status->last_hard_state_change = h->get_last_hard_state_change();
    host_status->last_notification = h->get_last_notification();
    host_status->notification_number = h->get_notification_number();
    host_status->last_state_change = h->get_last_state_change();
    host_status->last_time_down = h->get_last_time_down();
    host_status->last_time_unreachable = h->get_last_time_unreachable();
    host_status->last_time_up = h->get_last_time_up();
    host_status->last_update = time(nullptr);
    host_status->latency = h->get_latency();
    host_status->max_check_attempts = h->max_check_attempts();
    host_status->next_check = h->get_next_check();
    host_status->next_notification = h->get_next_notification();
    host_status->no_more_notifications = h->get_no_more_notifications();
    host_status->notifications_enabled = h->get_notifications_enabled();
    host_status->obsess_over = h->obsess_over();
    if (!h->get_plugin_output().empty()) {
      host_status->output =
          misc::string::check_string_utf8(h->get_plugin_output());
      host_status->output.append("\n");
    }
    if (!h->get_long_plugin_output().empty())
      host_status->output.append(
          misc::string::check_string_utf8(h->get_long_plugin_output()));
    host_status->passive_checks_enabled = h->passive_checks_enabled();
    host_status->percent_state_change = h->get_percent_state_change();
    if (!h->get_perf_data().empty())
      host_status->perf_data =
          misc::string::check_string_utf8(h->get_perf_data());
    host_status->retry_interval = h->retry_interval();
    host_status->should_be_scheduled = h->get_should_be_scheduled();
    host_status->state_type =
        (h->has_been_checked() ? h->get_state_type() : engine::notifier::hard);

    // Send event(s).
    gl_publisher.write(host_status);

    // Acknowledgement event.
    auto it =
        gl_acknowledgements.find(std::make_pair(host_status->host_id, 0u));
    if (it != gl_acknowledgements.end() && !host_status->acknowledged) {
      if (!(!host_status->current_state  // !(OK or (normal ack and NOK))
            || (!it->second.is_sticky &&
                (host_status->current_state != it->second.state)))) {
        auto ack{std::make_shared<neb::acknowledgement>(it->second)};
        ack->deletion_time = time(nullptr);
        gl_publisher.write(ack);
      }
      gl_acknowledgements.erase(it);
    }
  } catch (std::exception const& e) {
    log_v2::neb()->error(
        "callbacks: error occurred while generating host status event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process host status data.
 *
 *  This function is called by Nagios when some host status data are available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_HOST_STATUS_DATA).
 *  @param[in] data          A pointer to a nebstruct_host_status_data
 *                           containing the host status data.
 *
 *  @return 0 on success.
 */
int neb::callback_pb_host_status(int callback_type, void* data) noexcept {
  // Log message.
  log_v2::neb()->info(
      "callbacks: generating pb host status check result event protobuf");
  (void)callback_type;

  const engine::host* eh{static_cast<engine::host*>(
      static_cast<nebstruct_host_status_data*>(data)->object_ptr)};

  auto h{std::make_shared<neb::pb_host_status>()};
  HostStatus& hscr = h.get()->mut_obj();

  hscr.set_host_id(eh->get_host_id());
  if (hscr.host_id() == 0)
    log_v2::neb()->error("could not find ID of host '{}'", eh->name());

  if (eh->problem_has_been_acknowledged())
    hscr.set_acknowledgement_type(eh->get_acknowledgement());
  else
    hscr.set_acknowledgement_type(AckType::NONE);

  hscr.set_check_type(static_cast<HostStatus_CheckType>(eh->get_check_type()));
  hscr.set_check_attempt(eh->get_current_attempt());
  hscr.set_state(static_cast<HostStatus_State>(
      eh->has_been_checked() ? eh->get_current_state() : 4));  // Pending state.
  hscr.set_execution_time(eh->get_execution_time());
  hscr.set_checked(eh->has_been_checked());
  hscr.set_flapping(eh->get_is_flapping());
  hscr.set_last_check(eh->get_last_check());
  hscr.set_last_hard_state(
      static_cast<HostStatus_State>(eh->get_last_hard_state()));
  hscr.set_last_hard_state_change(eh->get_last_hard_state_change());
  hscr.set_last_notification(eh->get_last_notification());
  hscr.set_notification_number(eh->get_notification_number());
  hscr.set_last_state_change(eh->get_last_state_change());
  hscr.set_last_time_down(eh->get_last_time_down());
  hscr.set_last_time_unreachable(eh->get_last_time_unreachable());
  hscr.set_last_time_up(eh->get_last_time_up());
  hscr.set_latency(eh->get_latency());
  hscr.set_next_check(eh->get_next_check());
  hscr.set_next_host_notification(eh->get_next_notification());
  hscr.set_no_more_notifications(eh->get_no_more_notifications());
  if (!eh->get_plugin_output().empty())
    hscr.set_output(misc::string::check_string_utf8(eh->get_plugin_output()));
  if (!eh->get_long_plugin_output().empty())
    hscr.set_output(
        misc::string::check_string_utf8(eh->get_long_plugin_output()));

  hscr.set_percent_state_change(eh->get_percent_state_change());
  if (!eh->get_perf_data().empty())
    hscr.set_perfdata(misc::string::check_string_utf8(eh->get_perf_data()));
  hscr.set_should_be_scheduled(eh->get_should_be_scheduled());
  hscr.set_state_type(static_cast<HostStatus_StateType>(
      eh->has_been_checked() ? eh->get_state_type() : engine::notifier::hard));
  hscr.set_scheduled_downtime_depth(eh->get_scheduled_downtime_depth());

  // Send event(s).
  gl_publisher.write(h);

  // Acknowledgement event.
  auto it = gl_acknowledgements.find(std::make_pair(hscr.host_id(), 0u));
  if (it != gl_acknowledgements.end() &&
      hscr.acknowledgement_type() == AckType::NONE) {
    if (!(!hscr.state()  // !(OK or (normal ack and NOK))
          || (!it->second.is_sticky && (hscr.state() != it->second.state)))) {
      auto ack = std::make_shared<neb::acknowledgement>(it->second);
      ack->deletion_time = time(nullptr);
      gl_publisher.write(ack);
    }
    gl_acknowledgements.erase(it);
  }
  return 0;
}

/**
 *  @brief Function that process log data.
 *
 *  This function is called by Nagios when some log data are available.
 *
 *  @param[in] callback_type Type of the callback (NEBCALLBACK_LOG_DATA).
 *  @param[in] data          A pointer to a nebstruct_log_data containing the
 *                           log data.
 *
 *  @return 0 on success.
 */
int neb::callback_log(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating log event");
  (void)callback_type;

  try {
    // In/Out variables.
    nebstruct_log_data const* log_data;
    auto le{std::make_shared<neb::log_entry>()};

    // Fill output var.
    log_data = static_cast<nebstruct_log_data*>(data);
    le->c_time = log_data->entry_time;
    le->poller_name = config::applier::state::instance().poller_name();
    if (log_data->data) {
      le->output = misc::string::check_string_utf8(log_data->data);
      set_log_data(*le, le->output.c_str());
    }

    // Send event.
    gl_publisher.write(le);
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process module data.
 *
 *  This function is called by Engine when some module data is
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_MODULE_DATA).
 *  @param[in] data          A pointer to a nebstruct_module_data
 *                           containing the module data.
 *
 *  @return 0 on success.
 */
int neb::callback_module(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->debug("callbacks: generating module event");
  (void)callback_type;

  try {
    // In/Out variables.
    nebstruct_module_data const* module_data;
    auto me{std::make_shared<neb::module>()};

    // Fill output var.
    module_data = static_cast<nebstruct_module_data*>(data);
    if (module_data->module) {
      me->poller_id = config::applier::state::instance().poller_id();
      me->filename = misc::string::check_string_utf8(module_data->module);
      if (module_data->args)
        me->args = misc::string::check_string_utf8(module_data->args);
      me->loaded = !(module_data->type == NEBTYPE_MODULE_DELETE);
      me->should_be_loaded = true;

      // Send events.
      gl_publisher.write(me);
    }
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process process data.
 *
 *  This function is called by Nagios when some process data is available.
 *
 *  @param[in] callback_type Type of the callback (NEBCALLBACK_PROCESS_DATA).
 *  @param[in] data          A pointer to a nebstruct_process_data containing
 *                           the process data.
 *
 *  @return 0 on success.
 */
int neb::callback_process(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->debug("callbacks: process event callback");
  (void)callback_type;

  try {
    // Input variables.
    nebstruct_process_data const* process_data;
    static time_t start_time;

    // Check process event type.
    process_data = static_cast<nebstruct_process_data*>(data);
    if (NEBTYPE_PROCESS_EVENTLOOPSTART == process_data->type) {
      log_v2::neb()->info("callbacks: generating process start event");

      // Parse configuration file.
      std::tuple<uint16_t, uint16_t, uint16_t> bbdo_version;
      try {
        config::parser parsr;
        config::state conf{parsr.parse(gl_configuration_file)};

        bbdo_version = conf.bbdo_version();

        // Apply resulting configuration.
        config::applier::state::instance().apply(conf);

      } catch (msg_fmt const& e) {
        log_v2::neb()->info(e.what());
        return 0;
      }

      if (std::get<0>(bbdo_version) > 2) {
        // Register callbacks.
        log_v2::neb()->debug(
            "callbacks: registering callbacks for new BBDO version");
        for (uint32_t i = 0;
             i < sizeof(gl_pb_callbacks) / sizeof(*gl_pb_callbacks); ++i)
          gl_registered_callbacks.emplace_back(std::make_unique<callback>(
              gl_pb_callbacks[i].macro, gl_mod_handle,
              gl_pb_callbacks[i].callback));
      } else {
        // Register callbacks.
        log_v2::neb()->debug(
            "callbacks: registering callbacks for old BBDO version");
        for (uint32_t i(0); i < sizeof(gl_callbacks) / sizeof(*gl_callbacks);
             ++i)
          gl_registered_callbacks.emplace_back(std::make_unique<callback>(
              gl_callbacks[i].macro, gl_mod_handle, gl_callbacks[i].callback));
      }

      // Register Engine-specific callbacks.
      if (gl_mod_flags & NEBMODULE_ENGINE) {
        if (std::get<0>(bbdo_version) > 2) {
          // Register engine callbacks.
          log_v2::neb()->debug(
              "callbacks: registering callbacks for new BBDO version");
          for (uint32_t i = 0; i < sizeof(gl_pb_engine_callbacks) /
                                       sizeof(*gl_pb_engine_callbacks);
               ++i)
            gl_registered_callbacks.emplace_back(std::make_unique<callback>(
                gl_pb_engine_callbacks[i].macro, gl_mod_handle,
                gl_pb_engine_callbacks[i].callback));
        } else {
          // Register engine callbacks.
          log_v2::neb()->debug(
              "callbacks: registering callbacks for old BBDO version");
          for (uint32_t i = 0;
               i < sizeof(gl_engine_callbacks) / sizeof(*gl_engine_callbacks);
               ++i)
            gl_registered_callbacks.emplace_back(std::make_unique<callback>(
                gl_engine_callbacks[i].macro, gl_mod_handle,
                gl_engine_callbacks[i].callback));
        }
      }

      // Output variable.
      auto instance{std::make_shared<neb::instance>()};
      instance->poller_id = config::applier::state::instance().poller_id();
      instance->engine = "Centreon Engine";
      instance->is_running = true;
      instance->name = config::applier::state::instance().poller_name();
      instance->pid = getpid();
      instance->program_start = time(nullptr);
      instance->version = get_program_version();
      start_time = instance->program_start;

      // Send initial event and then configuration.
      gl_publisher.write(instance);
      if (std::get<0>(bbdo_version) > 2)
        send_initial_pb_configuration();
      else
        send_initial_configuration();
    } else if (NEBTYPE_PROCESS_EVENTLOOPEND == process_data->type) {
      log_v2::neb()->info("callbacks: generating process end event");
      // Output variable.
      auto instance{std::make_shared<neb::instance>()};

      // Fill output var.
      instance->poller_id = config::applier::state::instance().poller_id();
      instance->engine = "Centreon Engine";
      instance->is_running = false;
      instance->name = config::applier::state::instance().poller_name();
      instance->pid = getpid();
      instance->program_end = time(nullptr);
      instance->program_start = start_time;
      instance->version = get_program_version();

      // Send event.
      gl_publisher.write(instance);
    }
  }
  // Avoid exception propagation in C code.
  catch (...) {
    unregister_callbacks();
  }
  return 0;
}

/**
 *  @brief Function that process instance status data.
 *
 *  This function is called by Nagios when some instance status data are
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_PROGRAM_STATUS_DATA).
 *  @param[in] data          A pointer to a nebstruct_program_status_data
 *                           containing the program status data.
 *
 *  @return 0 on success.
 */
int neb::callback_program_status(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating instance status event");
  (void)callback_type;

  try {
    // In/Out variables.
    auto is{std::make_shared<neb::instance_status>()};

    // Fill output var.
    nebstruct_program_status_data const* program_status_data =
        static_cast<nebstruct_program_status_data*>(data);
    is->poller_id = config::applier::state::instance().poller_id();
    is->active_host_checks_enabled =
        program_status_data->active_host_checks_enabled;
    is->active_service_checks_enabled =
        program_status_data->active_service_checks_enabled;
    is->check_hosts_freshness = check_host_freshness;
    is->check_services_freshness = check_service_freshness;
    is->event_handler_enabled = program_status_data->event_handlers_enabled;
    is->flap_detection_enabled = program_status_data->flap_detection_enabled;
    if (!program_status_data->global_host_event_handler.empty())
      is->global_host_event_handler = misc::string::check_string_utf8(
          program_status_data->global_host_event_handler);
    if (!program_status_data->global_service_event_handler.empty())
      is->global_service_event_handler = misc::string::check_string_utf8(
          program_status_data->global_service_event_handler);
    is->last_alive = time(nullptr);
    is->last_command_check = program_status_data->last_command_check;
    is->notifications_enabled = program_status_data->notifications_enabled;
    is->obsess_over_hosts = program_status_data->obsess_over_hosts;
    is->obsess_over_services = program_status_data->obsess_over_services;
    is->passive_host_checks_enabled =
        program_status_data->passive_host_checks_enabled;
    is->passive_service_checks_enabled =
        program_status_data->passive_service_checks_enabled;

    // Send event.
    gl_publisher.write(is);
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process relation data.
 *
 *  This function is called by Engine when some relation data is
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_RELATION_DATA).
 *  @param[in] data          Pointer to a nebstruct_relation_data
 *                           containing the relationship.
 *
 *  @return 0 on success.
 */
int neb::callback_relation(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating relation event");
  (void)callback_type;

  try {
    // Input variable.
    nebstruct_relation_data const* relation(
        static_cast<nebstruct_relation_data*>(data));

    // Host parent.
    if ((NEBTYPE_PARENT_ADD == relation->type) ||
        (NEBTYPE_PARENT_DELETE == relation->type)) {
      if (relation->hst && relation->dep_hst && !relation->svc &&
          !relation->dep_svc) {
        // Find host IDs.
        int host_id;
        int parent_id;
        {
          host_id = engine::get_host_id(relation->dep_hst->name());
          parent_id = engine::get_host_id(relation->hst->name());
        }
        if (host_id && parent_id) {
          // Generate parent event.
          auto new_host_parent{std::make_shared<host_parent>()};
          new_host_parent->enabled = (relation->type != NEBTYPE_PARENT_DELETE);
          new_host_parent->host_id = host_id;
          new_host_parent->parent_id = parent_id;

          // Send event.
          log_v2::neb()->info("callbacks: host {} is parent of host {}",
                              new_host_parent->parent_id,
                              new_host_parent->host_id);
          neb::gl_publisher.write(new_host_parent);
        }
      }
    }
  }
  // Avoid exception propagation to C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process service data.
 *
 *  This function is called by Engine when some service data is
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_ADAPTIVE_SERVICE_DATA).
 *  @param[in] data          A pointer to a
 *                           nebstruct_adaptive_service_data containing
 *                           the service data.
 *
 *  @return 0 on success.
 */
int neb::callback_service(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating service event");
  (void)callback_type;

  try {
    // In/Out variables.
    const nebstruct_adaptive_service_data* service_data(
        static_cast<nebstruct_adaptive_service_data*>(data));
    if (service_data->flags & NEBATTR_BBDO3_ONLY)
      return 0;
    engine::service const* s(
        static_cast<engine::service*>(service_data->object_ptr));
    auto my_service{std::make_shared<neb::service>()};

    // Fill output var.
    my_service->acknowledged = s->problem_has_been_acknowledged();
    my_service->acknowledgement_type = s->get_acknowledgement();
    if (!s->get_action_url().empty())
      my_service->action_url =
          misc::string::check_string_utf8(s->get_action_url());
    my_service->active_checks_enabled = s->active_checks_enabled();
    if (!s->check_command().empty())
      my_service->check_command =
          misc::string::check_string_utf8(s->check_command());
    my_service->check_freshness = s->check_freshness_enabled();
    my_service->check_interval = s->check_interval();
    if (!s->check_period().empty())
      my_service->check_period = s->check_period();
    my_service->check_type = s->get_check_type();
    my_service->current_check_attempt = s->get_current_attempt();
    my_service->current_state =
        (s->has_been_checked() ? s->get_current_state() : 4);  // Pending state.
    my_service->default_active_checks_enabled = s->active_checks_enabled();
    my_service->default_event_handler_enabled = s->event_handler_enabled();
    my_service->default_flap_detection_enabled = s->flap_detection_enabled();
    my_service->default_notifications_enabled = s->get_notifications_enabled();
    my_service->default_passive_checks_enabled = s->passive_checks_enabled();
    my_service->downtime_depth = s->get_scheduled_downtime_depth();
    if (!s->get_display_name().empty())
      my_service->display_name =
          misc::string::check_string_utf8(s->get_display_name());
    my_service->enabled = (service_data->type != NEBTYPE_SERVICE_DELETE);
    if (!s->event_handler().empty())
      my_service->event_handler =
          misc::string::check_string_utf8(s->event_handler());
    my_service->event_handler_enabled = s->event_handler_enabled();
    my_service->execution_time = s->get_execution_time();
    my_service->first_notification_delay = s->get_first_notification_delay();
    my_service->notification_number = s->get_notification_number();
    my_service->flap_detection_enabled = s->flap_detection_enabled();
    my_service->flap_detection_on_critical =
        s->get_flap_detection_on(engine::notifier::critical);
    my_service->flap_detection_on_ok =
        s->get_flap_detection_on(engine::notifier::ok);
    my_service->flap_detection_on_unknown =
        s->get_flap_detection_on(engine::notifier::unknown);
    my_service->flap_detection_on_warning =
        s->get_flap_detection_on(engine::notifier::warning);
    my_service->freshness_threshold = s->get_freshness_threshold();
    my_service->has_been_checked = s->has_been_checked();
    my_service->high_flap_threshold = s->get_high_flap_threshold();
    if (!s->get_hostname().empty())
      my_service->host_name =
          misc::string::check_string_utf8(s->get_hostname());
    if (!s->get_icon_image().empty())
      my_service->icon_image =
          misc::string::check_string_utf8(s->get_icon_image());
    if (!s->get_icon_image_alt().empty())
      my_service->icon_image_alt =
          misc::string::check_string_utf8(s->get_icon_image_alt());
    my_service->is_flapping = s->get_is_flapping();
    my_service->is_volatile = s->get_is_volatile();
    my_service->last_check = s->get_last_check();
    my_service->last_hard_state = s->get_last_hard_state();
    my_service->last_hard_state_change = s->get_last_hard_state_change();
    my_service->last_notification = s->get_last_notification();
    my_service->last_state_change = s->get_last_state_change();
    my_service->last_time_critical = s->get_last_time_critical();
    my_service->last_time_ok = s->get_last_time_ok();
    my_service->last_time_unknown = s->get_last_time_unknown();
    my_service->last_time_warning = s->get_last_time_warning();
    my_service->last_update = time(nullptr);
    my_service->latency = s->get_latency();
    my_service->low_flap_threshold = s->get_low_flap_threshold();
    my_service->max_check_attempts = s->max_check_attempts();
    my_service->next_check = s->get_next_check();
    my_service->next_notification = s->get_next_notification();
    my_service->no_more_notifications = s->get_no_more_notifications();
    if (!s->get_notes().empty())
      my_service->notes = misc::string::check_string_utf8(s->get_notes());
    if (!s->get_notes_url().empty())
      my_service->notes_url =
          misc::string::check_string_utf8(s->get_notes_url());
    my_service->notifications_enabled = s->get_notifications_enabled();
    my_service->notification_interval = s->get_notification_interval();
    if (!s->notification_period().empty())
      my_service->notification_period = s->notification_period();
    my_service->notify_on_critical =
        s->get_notify_on(engine::notifier::critical);
    my_service->notify_on_downtime =
        s->get_notify_on(engine::notifier::downtime);
    my_service->notify_on_flapping =
        s->get_notify_on(engine::notifier::flappingstart);
    my_service->notify_on_recovery = s->get_notify_on(engine::notifier::ok);
    my_service->notify_on_unknown = s->get_notify_on(engine::notifier::unknown);
    my_service->notify_on_warning = s->get_notify_on(engine::notifier::warning);
    my_service->obsess_over = s->obsess_over();
    if (!s->get_plugin_output().empty()) {
      my_service->output =
          misc::string::check_string_utf8(s->get_plugin_output());
      my_service->output.append("\n");
    }
    if (!s->get_long_plugin_output().empty())
      my_service->output.append(
          misc::string::check_string_utf8(s->get_long_plugin_output()));
    my_service->passive_checks_enabled = s->passive_checks_enabled();
    my_service->percent_state_change = s->get_percent_state_change();
    if (!s->get_perf_data().empty())
      my_service->perf_data =
          misc::string::check_string_utf8(s->get_perf_data());
    my_service->retain_nonstatus_information =
        s->get_retain_nonstatus_information();
    my_service->retain_status_information = s->get_retain_status_information();
    my_service->retry_interval = s->retry_interval();
    if (!s->get_description().empty())
      my_service->service_description =
          misc::string::check_string_utf8(s->get_description());
    my_service->should_be_scheduled = s->get_should_be_scheduled();
    my_service->stalk_on_critical = s->get_stalk_on(engine::notifier::critical);
    my_service->stalk_on_ok = s->get_stalk_on(engine::notifier::ok);
    my_service->stalk_on_unknown = s->get_stalk_on(engine::notifier::unknown);
    my_service->stalk_on_warning = s->get_stalk_on(engine::notifier::warning);
    my_service->state_type =
        (s->has_been_checked() ? s->get_state_type() : engine::notifier::hard);

    // Search host ID and service ID.
    std::pair<uint64_t, uint64_t> p;
    p = engine::get_host_and_service_id(s->get_hostname(),
                                        s->get_description());
    my_service->host_id = p.first;
    my_service->service_id = p.second;
    if (my_service->host_id && my_service->service_id) {
      // Send service event.
      log_v2::neb()->info("callbacks: new service {} ('{}') on host {}",
                          my_service->service_id,
                          my_service->service_description, my_service->host_id);
      neb::gl_publisher.write(my_service);

      /* No need to send this service custom variables changes, custom
       * variables are managed in a different loop. */
    } else
      log_v2::neb()->error(
          "callbacks: service has no host ID or no service ID (yet) (host "
          "'{}', service '{}')",
          (!s->get_hostname().empty() ? my_service->host_name : "(unknown)"),
          (!s->get_description().empty() ? my_service->service_description
                                         : "(unknown)"));
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  @brief Function that process protobuf service data.
 *
 *  This function is called by Engine when some service data is
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_ADAPTIVE_SERVICE_DATA).
 *  @param[in] data          A pointer to a
 *                           nebstruct_adaptive_service_data containing
 *                           the service data.
 *
 *  @return 0 on success.
 */
int neb::callback_pb_service(int callback_type, void* data) {
  log_v2::neb()->info("callbacks: generating pb service event protobuf");

  nebstruct_adaptive_service_data* ds =
      static_cast<nebstruct_adaptive_service_data*>(data);
  const engine::service* es{static_cast<engine::service*>(ds->object_ptr)};

  log_v2::neb()->trace("modified_attribute = {}", ds->modified_attribute);
  if (ds->type == NEBTYPE_ADAPTIVESERVICE_UPDATE &&
      ds->modified_attribute != MODATTR_ALL) {
    auto s{std::make_shared<neb::pb_adaptive_service>()};
    AdaptiveService& srv = s.get()->mut_obj();
    if (ds->modified_attribute & MODATTR_NOTIFICATIONS_ENABLED)
      srv.set_notify(es->get_notifications_enabled());
    else if (ds->modified_attribute & MODATTR_ACTIVE_CHECKS_ENABLED) {
      srv.set_active_checks(es->active_checks_enabled());
      srv.set_should_be_scheduled(es->get_should_be_scheduled());
    } else if (ds->modified_attribute & MODATTR_PASSIVE_CHECKS_ENABLED)
      srv.set_passive_checks(es->passive_checks_enabled());
    else if (ds->modified_attribute & MODATTR_EVENT_HANDLER_ENABLED)
      srv.set_event_handler_enabled(es->event_handler_enabled());
    else if (ds->modified_attribute & MODATTR_FLAP_DETECTION_ENABLED)
      srv.set_flap_detection_enabled(es->flap_detection_enabled());
    else if (ds->modified_attribute & MODATTR_OBSESSIVE_HANDLER_ENABLED)
      srv.set_obsess_over_service(es->obsess_over());
    else if (ds->modified_attribute & MODATTR_EVENT_HANDLER_COMMAND)
      srv.set_event_handler(
          misc::string::check_string_utf8(es->event_handler()));
    else if (ds->modified_attribute & MODATTR_CHECK_COMMAND)
      srv.set_check_command(
          misc::string::check_string_utf8(es->check_command()));
    else if (ds->modified_attribute & MODATTR_NORMAL_CHECK_INTERVAL)
      srv.set_check_interval(es->check_interval());
    else if (ds->modified_attribute & MODATTR_RETRY_CHECK_INTERVAL)
      srv.set_retry_interval(es->retry_interval());
    else if (ds->modified_attribute & MODATTR_MAX_CHECK_ATTEMPTS)
      srv.set_max_check_attempts(es->max_check_attempts());
    else if (ds->modified_attribute & MODATTR_FRESHNESS_CHECKS_ENABLED)
      srv.set_check_freshness(es->check_freshness_enabled());
    else if (ds->modified_attribute & MODATTR_CHECK_TIMEPERIOD)
      srv.set_check_period(es->check_period());
    else if (ds->modified_attribute & MODATTR_NOTIFICATION_TIMEPERIOD)
      srv.set_notification_period(es->notification_period());
    else {
      log_v2::neb()->error("callbacks: adaptive service not implemented.");
      assert(1 == 0);
    }
    std::pair<uint64_t, uint64_t> p{engine::get_host_and_service_id(
        es->get_hostname(), es->get_description())};
    if (p.first && p.second) {
      srv.set_host_id(p.first);
      srv.set_service_id(p.second);
      // Send service event.
      log_v2::neb()->info("callbacks: new service {} ('{}') on host {}",
                          srv.service_id(), es->get_description(),
                          srv.host_id());
      neb::gl_publisher.write(s);

      /* No need to send this service custom variables changes, custom
       * variables are managed in a different loop. */
    } else
      log_v2::neb()->error(
          "callbacks: service has no host ID or no service ID (yet) (host "
          "'{}', service '{}')",
          !es->get_hostname().empty() ? es->get_hostname() : "(unknown)",
          !es->get_description().empty() ? es->get_description() : "(unknown)");
  } else {
    auto s{std::make_shared<neb::pb_service>()};
    Service& srv = s.get()->mut_obj();

    // Fill output var.
    srv.set_acknowledged(es->problem_has_been_acknowledged());
    srv.set_acknowledgement_type(es->get_acknowledgement());
    if (!es->get_action_url().empty())
      srv.set_action_url(misc::string::check_string_utf8(es->get_action_url()));
    srv.set_active_checks(es->active_checks_enabled());
    if (!es->check_command().empty())
      srv.set_check_command(
          misc::string::check_string_utf8(es->check_command()));
    srv.set_check_freshness(es->check_freshness_enabled());
    srv.set_check_interval(es->check_interval());
    if (!es->check_period().empty())
      srv.set_check_period(es->check_period());
    srv.set_check_type(static_cast<Service_CheckType>(es->get_check_type()));
    srv.set_check_attempt(es->get_current_attempt());
    srv.set_state(static_cast<Service_State>(es->has_been_checked()
                                                 ? es->get_current_state()
                                                 : 4));  // Pending state.
    srv.set_default_active_checks(es->active_checks_enabled());
    srv.set_default_event_handler_enabled(es->event_handler_enabled());
    srv.set_default_flap_detection(es->flap_detection_enabled());
    srv.set_default_notify(es->get_notifications_enabled());
    srv.set_default_passive_checks(es->passive_checks_enabled());
    srv.set_scheduled_downtime_depth(es->get_scheduled_downtime_depth());
    if (!es->get_display_name().empty())
      srv.set_display_name(
          misc::string::check_string_utf8(es->get_display_name()));
    srv.set_enabled(static_cast<nebstruct_adaptive_service_data*>(data)->type !=
                    NEBTYPE_SERVICE_DELETE);
    if (!es->event_handler().empty())
      srv.set_event_handler(
          misc::string::check_string_utf8(es->event_handler()));
    srv.set_event_handler_enabled(es->event_handler_enabled());
    srv.set_execution_time(es->get_execution_time());
    srv.set_first_notification_delay(es->get_first_notification_delay());
    srv.set_notification_number(es->get_notification_number());
    srv.set_flap_detection(es->flap_detection_enabled());
    srv.set_flap_detection_on_critical(
        es->get_flap_detection_on(engine::notifier::critical));
    srv.set_flap_detection_on_ok(
        es->get_flap_detection_on(engine::notifier::ok));
    srv.set_flap_detection_on_unknown(
        es->get_flap_detection_on(engine::notifier::unknown));
    srv.set_flap_detection_on_warning(
        es->get_flap_detection_on(engine::notifier::warning));
    srv.set_freshness_threshold(es->get_freshness_threshold());
    srv.set_checked(es->has_been_checked());
    srv.set_high_flap_threshold(es->get_high_flap_threshold());
    if (!es->get_description().empty())
      srv.set_description(
          misc::string::check_string_utf8(es->get_description()));

    if (!es->get_hostname().empty()) {
      std::string name{misc::string::check_string_utf8(es->get_hostname())};
      switch (es->get_service_type()) {
        case com::centreon::engine::service_type::METASERVICE: {
          srv.set_type(METASERVICE);
          uint64_t iid = 0;
          for (auto c = srv.description().begin() + 5;
               c != srv.description().end(); ++c) {
            if (!isdigit(*c)) {
              log_v2::neb()->error(
                  "callbacks: service ('{}', '{}') looks like a meta-service "
                  "but its name is malformed",
                  name, srv.description());
              break;
            }
            iid = 10 * iid + (*c - '0');
          }
          srv.set_internal_id(iid);
        } break;
        case com::centreon::engine::service_type::BA: {
          srv.set_type(BA);
          uint64_t iid = 0;
          for (auto c = srv.description().begin() + 3;
               c != srv.description().end(); ++c) {
            if (!isdigit(*c)) {
              log_v2::neb()->error(
                  "callbacks: service ('{}', '{}') looks like a "
                  "business-activity but its name is malformed",
                  name, srv.description());
              break;
            }
            iid = 10 * iid + (*c - '0');
          }
          srv.set_internal_id(iid);
        } break;
        case com::centreon::engine::service_type::ANOMALY_DETECTION:
          srv.set_type(ANOMALY_DETECTION);
          {
            auto ad =
                static_cast<const com::centreon::engine::anomalydetection*>(es);
            srv.set_internal_id(ad->get_internal_id());
          }
          break;
        default:
          srv.set_type(SERVICE);
          break;
      }
      *srv.mutable_host_name() = std::move(name);
    }
    if (!es->get_icon_image().empty())
      *srv.mutable_icon_image() =
          misc::string::check_string_utf8(es->get_icon_image());
    if (!es->get_icon_image_alt().empty())
      *srv.mutable_icon_image_alt() =
          misc::string::check_string_utf8(es->get_icon_image_alt());
    srv.set_flapping(es->get_is_flapping());
    srv.set_is_volatile(es->get_is_volatile());
    srv.set_last_check(es->get_last_check());
    srv.set_last_hard_state(
        static_cast<Service_State>(es->get_last_hard_state()));
    srv.set_last_hard_state_change(es->get_last_hard_state_change());
    srv.set_last_notification(es->get_last_notification());
    srv.set_last_state_change(es->get_last_state_change());
    srv.set_last_time_critical(es->get_last_time_critical());
    srv.set_last_time_ok(es->get_last_time_ok());
    srv.set_last_time_unknown(es->get_last_time_unknown());
    srv.set_last_time_warning(es->get_last_time_warning());
    srv.set_last_update(time(nullptr));
    srv.set_latency(es->get_latency());
    srv.set_low_flap_threshold(es->get_low_flap_threshold());
    srv.set_max_check_attempts(es->max_check_attempts());
    srv.set_next_check(es->get_next_check());
    srv.set_next_notification(es->get_next_notification());
    srv.set_no_more_notifications(es->get_no_more_notifications());
    if (!es->get_notes().empty())
      srv.set_notes(misc::string::check_string_utf8(es->get_notes()));
    if (!es->get_notes_url().empty())
      *srv.mutable_notes_url() =
          misc::string::check_string_utf8(es->get_notes_url());
    srv.set_notify(es->get_notifications_enabled());
    srv.set_notification_interval(es->get_notification_interval());
    if (!es->notification_period().empty())
      srv.set_notification_period(es->notification_period());
    srv.set_notify_on_critical(es->get_notify_on(engine::notifier::critical));
    srv.set_notify_on_downtime(es->get_notify_on(engine::notifier::downtime));
    srv.set_notify_on_flapping(
        es->get_notify_on(engine::notifier::flappingstart));
    srv.set_notify_on_recovery(es->get_notify_on(engine::notifier::ok));
    srv.set_notify_on_unknown(es->get_notify_on(engine::notifier::unknown));
    srv.set_notify_on_warning(es->get_notify_on(engine::notifier::warning));
    srv.set_obsess_over_service(es->obsess_over());
    if (!es->get_plugin_output().empty())
      *srv.mutable_output() =
          misc::string::check_string_utf8(es->get_plugin_output());
    if (!es->get_long_plugin_output().empty())
      *srv.mutable_long_output() =
          misc::string::check_string_utf8(es->get_long_plugin_output());
    srv.set_passive_checks(es->passive_checks_enabled());
    srv.set_percent_state_change(es->get_percent_state_change());
    if (!es->get_perf_data().empty())
      *srv.mutable_perfdata() =
          misc::string::check_string_utf8(es->get_perf_data());
    srv.set_retain_nonstatus_information(
        es->get_retain_nonstatus_information());
    srv.set_retain_status_information(es->get_retain_status_information());
    srv.set_retry_interval(es->retry_interval());
    srv.set_should_be_scheduled(es->get_should_be_scheduled());
    srv.set_stalk_on_critical(es->get_stalk_on(engine::notifier::critical));
    srv.set_stalk_on_ok(es->get_stalk_on(engine::notifier::ok));
    srv.set_stalk_on_unknown(es->get_stalk_on(engine::notifier::unknown));
    srv.set_stalk_on_warning(es->get_stalk_on(engine::notifier::warning));
    srv.set_state_type(static_cast<Service_StateType>(
        es->has_been_checked() ? es->get_state_type()
                               : engine::notifier::hard));
    srv.set_severity_id(es->get_severity() ? es->get_severity()->id() : 0);
    srv.set_icon_id(es->get_icon_id());

    for (auto& tg : es->tags()) {
      TagInfo* ti = srv.mutable_tags()->Add();
      ti->set_id(tg->id());
      ti->set_type(static_cast<TagType>(tg->type()));
    }

    // Search host ID and service ID.
    std::pair<uint64_t, uint64_t> p;
    p = engine::get_host_and_service_id(es->get_hostname(),
                                        es->get_description());
    srv.set_host_id(p.first);
    srv.set_service_id(p.second);
    if (srv.host_id() && srv.service_id())
      log_v2::neb()->debug("callbacks: service ({}, {}) has a severity id {}",
                           srv.host_id(), srv.service_id(), srv.severity_id());
    if (srv.host_id() && srv.service_id()) {
      // Send service event.
      log_v2::neb()->info("callbacks: new service {} ('{}') on host {}",
                          srv.service_id(), srv.description(), srv.host_id());
      neb::gl_publisher.write(s);

      /* No need to send this service custom variables changes, custom
       * variables are managed in a different loop. */
    } else
      log_v2::neb()->error(
          "callbacks: service has no host ID or no service ID (yet) (host "
          "'{}', service '{}')",
          (!es->get_hostname().empty() ? srv.host_name() : "(unknown)"),
          (!es->get_description().empty() ? srv.description() : "(unknown)"));
  }
  return 0;
}

/**
 *  @brief Function that process service check data.
 *
 *  This function is called by Nagios when some service check data are
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_SERVICE_CHECK_DATA).
 *  @param[in] data          A pointer to a nebstruct_service_check_data
 *                           containing the service check data.
 *
 *  @return 0 on success.
 */
int neb::callback_service_check(int callback_type, void* data) {
  const nebstruct_service_check_data* scdata =
      static_cast<nebstruct_service_check_data*>(data);

  /* For each check, this event is received three times one precheck, one
   * initiate and one processed. We just keep the initiate one. At the
   * processed one we also received the service status. */
  if (scdata->type != NEBTYPE_SERVICECHECK_INITIATE)
    return 0;

  // Log message.
  log_v2::neb()->info("callbacks: generating service check event");
  (void)callback_type;

  try {
    // In/Out variables.
    auto service_check{std::make_shared<neb::service_check>()};
    // Fill output var.
    engine::service* s{static_cast<engine::service*>(scdata->object_ptr)};
    if (scdata->command_line) {
      service_check->active_checks_enabled = s->active_checks_enabled();
      service_check->check_type = scdata->check_type;
      service_check->command_line =
          misc::string::check_string_utf8(scdata->command_line);
      if (!scdata->host_id)
        throw msg_fmt("host without id");
      if (!scdata->service_id)
        throw msg_fmt("service without id");
      service_check->host_id = scdata->host_id;
      service_check->service_id = scdata->service_id;
      service_check->next_check = s->get_next_check();

      // Send event.
      gl_publisher.write(service_check);
    }
  } catch (std::exception const& e) {
    log_v2::neb()->error(
        "callbacks: error occurred while generating service check event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 * @brief Process adaptive severity data.
 *
 * @param callback_type Callback type (NEBCALLBACK_ADAPTIVE_SEVERITY_DATA).
 * @param data A pointer to a nebstruct_adaptive_severity_data.
 *
 * @return 0 on success.
 */
int32_t neb::callback_severity(int callback_type __attribute__((unused)),
                               void* data) noexcept {
  log_v2::neb()->info("callbacks: generating protobuf severity event");

  nebstruct_adaptive_severity_data* ds =
      static_cast<nebstruct_adaptive_severity_data*>(data);
  const engine::severity* es{static_cast<engine::severity*>(ds->object_ptr)};

  auto s{std::make_shared<neb::pb_severity>()};
  Severity& sv = s.get()->mut_obj();
  switch (ds->type) {
    case NEBTYPE_SEVERITY_ADD:
      log_v2::neb()->info("callbacks: new severity");
      sv.set_action(Severity_Action_ADD);
      break;
    case NEBTYPE_SEVERITY_DELETE:
      log_v2::neb()->info("callbacks: removed severity");
      sv.set_action(Severity_Action_DELETE);
      break;
    case NEBTYPE_SEVERITY_UPDATE:
      log_v2::neb()->info("callbacks: modified severity");
      sv.set_action(Severity_Action_MODIFY);
      break;
    default:
      log_v2::neb()->error(
          "callbacks: protobuf severity event action must be among ADD, MODIFY "
          "or DELETE");
      return 1;
  }
  sv.set_id(es->id());
  sv.set_poller_id(config::applier::state::instance().poller_id());
  sv.set_level(es->level());
  sv.set_icon_id(es->icon_id());
  sv.set_name(es->name());
  sv.set_type(static_cast<com::centreon::broker::Severity_Type>(es->type()));

  // Send event(s).
  gl_publisher.write(s);
  return 0;
}

/**
 * @brief Process adaptive tag data.
 *
 * @param callback_type Callback type (NEBCALLBACK_ADAPTIVE_SEVERITY_DATA).
 * @param data A pointer to a nebstruct_adaptive_tag_data.
 *
 * @return 0 on success.
 */
int32_t neb::callback_tag(int callback_type __attribute__((unused)),
                          void* data) noexcept {
  log_v2::neb()->info("callbacks: generating protobuf tag event");

  nebstruct_adaptive_tag_data* ds =
      static_cast<nebstruct_adaptive_tag_data*>(data);
  const engine::tag* et{static_cast<engine::tag*>(ds->object_ptr)};

  auto t{std::make_shared<neb::pb_tag>()};
  Tag& tg = t.get()->mut_obj();
  switch (ds->type) {
    case NEBTYPE_TAG_ADD:
      log_v2::neb()->info("callbacks: new tag");
      tg.set_action(Tag_Action_ADD);
      break;
    case NEBTYPE_TAG_DELETE:
      log_v2::neb()->info("callbacks: removed tag");
      tg.set_action(Tag_Action_DELETE);
      break;
    case NEBTYPE_TAG_UPDATE:
      log_v2::neb()->info("callbacks: modified tag");
      tg.set_action(Tag_Action_MODIFY);
      break;
    default:
      log_v2::neb()->error(
          "callbacks: protobuf tag event action must be among ADD, MODIFY "
          "or DELETE");
      return 1;
  }
  tg.set_id(et->id());
  tg.set_poller_id(config::applier::state::instance().poller_id());
  switch (et->type()) {
    case engine::tag::hostcategory:
      tg.set_type(HOSTCATEGORY);
      break;
    case engine::tag::servicecategory:
      tg.set_type(SERVICECATEGORY);
      break;
    case engine::tag::hostgroup:
      tg.set_type(HOSTGROUP);
      break;
    case engine::tag::servicegroup:
      tg.set_type(SERVICEGROUP);
      break;
    default:
      log_v2::neb()->error(
          "callbacks: protobuf tag event type must be among HOSTCATEGORY, "
          "SERVICECATEGORY, HOSTGROUP pr SERVICEGROUP");
      return 1;
  }
  tg.set_name(et->name());

  // Send event(t).
  gl_publisher.write(t);
  return 0;
}

int32_t neb::callback_pb_service_status(int callback_type
                                        __attribute__((unused)),
                                        void* data) noexcept {
  log_v2::neb()->info(
      "callbacks: generating service status check result protobuf event");

  const engine::service* es{static_cast<engine::service*>(
      static_cast<nebstruct_service_status_data*>(data)->object_ptr)};

  auto s{std::make_shared<neb::pb_service_status>()};
  ServiceStatus& sscr = s.get()->mut_obj();

  sscr.set_host_id(es->get_host_id());
  sscr.set_service_id(es->get_service_id());
  if (es->get_host_id() == 0 || es->get_service_id() == 0)
    log_v2::neb()->error("could not find ID of service ('{}', '{}')",
                         es->get_hostname(), es->get_description());

  if (es->problem_has_been_acknowledged())
    sscr.set_acknowledgement_type(es->get_acknowledgement());
  else
    sscr.set_acknowledgement_type(AckType::NONE);

  sscr.set_check_type(
      static_cast<ServiceStatus_CheckType>(es->get_check_type()));
  sscr.set_check_attempt(es->get_current_attempt());
  sscr.set_state(static_cast<ServiceStatus_State>(
      es->has_been_checked() ? es->get_current_state() : 4));  // Pending state.
  sscr.set_execution_time(es->get_execution_time());
  sscr.set_checked(es->has_been_checked());
  sscr.set_flapping(es->get_is_flapping());
  sscr.set_last_check(es->get_last_check());
  sscr.set_last_hard_state(
      static_cast<ServiceStatus_State>(es->get_last_hard_state()));
  sscr.set_last_hard_state_change(es->get_last_hard_state_change());
  sscr.set_last_notification(es->get_last_notification());
  sscr.set_notification_number(es->get_notification_number());
  sscr.set_last_state_change(es->get_last_state_change());
  sscr.set_last_time_critical(es->get_last_time_critical());
  sscr.set_last_time_ok(es->get_last_time_ok());
  sscr.set_last_time_unknown(es->get_last_time_unknown());
  sscr.set_last_time_warning(es->get_last_time_warning());
  sscr.set_latency(es->get_latency());
  sscr.set_next_check(es->get_next_check());
  sscr.set_next_notification(es->get_next_notification());
  sscr.set_no_more_notifications(es->get_no_more_notifications());
  if (!es->get_plugin_output().empty())
    sscr.set_output(misc::string::check_string_utf8(es->get_plugin_output()));
  if (!es->get_long_plugin_output().empty())
    sscr.set_long_output(
        misc::string::check_string_utf8(es->get_long_plugin_output()));
  sscr.set_percent_state_change(es->get_percent_state_change());
  if (!es->get_perf_data().empty()) {
    sscr.set_perfdata(misc::string::check_string_utf8(es->get_perf_data()));
    log_v2::neb()->trace("callbacks: service ({}, {}) has perfdata <<{}>>",
                         es->get_host_id(), es->get_service_id(),
                         es->get_perf_data());
  } else {
    log_v2::neb()->trace("callbacks: service ({}, {}) has no perfdata",
                         es->get_host_id(), es->get_service_id());
  }
  sscr.set_should_be_scheduled(es->get_should_be_scheduled());
  sscr.set_state_type(static_cast<ServiceStatus_StateType>(
      es->has_been_checked() ? es->get_state_type() : engine::notifier::hard));
  sscr.set_scheduled_downtime_depth(es->get_scheduled_downtime_depth());

  if (!es->get_hostname().empty()) {
    if (strncmp(es->get_hostname().c_str(), "_Module_Meta", 13) == 0) {
      if (strncmp(es->get_description().c_str(), "meta_", 5) == 0) {
        sscr.set_type(METASERVICE);
        uint64_t iid = 0;
        for (auto c = es->get_description().begin() + 5;
             c != es->get_description().end(); ++c) {
          if (!isdigit(*c)) {
            log_v2::neb()->error(
                "callbacks: service ('{}', '{}') looks like a meta-service "
                "but its name is malformed",
                es->get_hostname(), es->get_description());
            break;
          }
          iid = 10 * iid + (*c - '0');
        }
        sscr.set_internal_id(iid);
      }
    } else if (strncmp(es->get_hostname().c_str(), "_Module_BAM", 11) == 0) {
      if (strncmp(es->get_description().c_str(), "ba_", 3) == 0) {
        sscr.set_type(BA);
        uint64_t iid = 0;
        for (auto c = es->get_description().begin() + 3;
             c != es->get_description().end(); ++c) {
          if (!isdigit(*c)) {
            log_v2::neb()->error(
                "callbacks: service ('{}', '{}') looks like a "
                "business-activity but its name is malformed",
                es->get_hostname(), es->get_description());
            break;
          }
          iid = 10 * iid + (*c - '0');
        }
        sscr.set_internal_id(iid);
      }
    }
  }
  // Send event(s).
  gl_publisher.write(s);

  // Acknowledgement event.
  auto it = gl_acknowledgements.find(
      std::make_pair(sscr.host_id(), sscr.service_id()));
  if (it != gl_acknowledgements.end() &&
      sscr.acknowledgement_type() == AckType::NONE) {
    if (!(!sscr.state()  // !(OK or (normal ack and NOK))
          || (!it->second.is_sticky && (sscr.state() != it->second.state)))) {
      auto ack = std::make_shared<neb::acknowledgement>(it->second);
      ack->deletion_time = time(nullptr);
      gl_publisher.write(ack);
    }
    gl_acknowledgements.erase(it);
  }
  return 0;
}

/**
 *  @brief Function that process service status data.
 *
 *  This function is called by Nagios when some service status data are
 *  available.
 *
 *  @param[in] callback_type Type of the callback
 *                           (NEBCALLBACK_SERVICE_STATUS_DATA).
 *  @param[in] data          A pointer to a nebstruct_service_status_data
 *                           containing the service status data.
 *
 *  @return 0 on success.
 */
int neb::callback_service_status(int callback_type, void* data) {
  // Log message.
  log_v2::neb()->info("callbacks: generating service status event");
  (void)callback_type;

  try {
    // In/Out variables.
    auto service_status{std::make_shared<neb::service_status>()};

    // Fill output var.
    engine::service const* s{static_cast<engine::service*>(
        static_cast<nebstruct_service_status_data*>(data)->object_ptr)};
    service_status->acknowledged = s->problem_has_been_acknowledged();
    service_status->acknowledgement_type = s->get_acknowledgement();
    service_status->active_checks_enabled = s->active_checks_enabled();
    if (!s->check_command().empty())
      service_status->check_command =
          misc::string::check_string_utf8(s->check_command());
    service_status->check_interval = s->check_interval();
    if (!s->check_period().empty())
      service_status->check_period = s->check_period();
    service_status->check_type = s->get_check_type();
    service_status->current_check_attempt = s->get_current_attempt();
    service_status->current_state =
        (s->has_been_checked() ? s->get_current_state() : 4);  // Pending state.
    service_status->downtime_depth = s->get_scheduled_downtime_depth();
    if (!s->event_handler().empty())
      service_status->event_handler =
          misc::string::check_string_utf8(s->event_handler());
    service_status->event_handler_enabled = s->event_handler_enabled();
    service_status->execution_time = s->get_execution_time();
    service_status->flap_detection_enabled = s->flap_detection_enabled();
    service_status->has_been_checked = s->has_been_checked();
    service_status->is_flapping = s->get_is_flapping();
    service_status->last_check = s->get_last_check();
    service_status->last_hard_state = s->get_last_hard_state();
    service_status->last_hard_state_change = s->get_last_hard_state_change();
    service_status->last_notification = s->get_last_notification();
    service_status->notification_number = s->get_notification_number();
    service_status->last_state_change = s->get_last_state_change();
    service_status->last_time_critical = s->get_last_time_critical();
    service_status->last_time_ok = s->get_last_time_ok();
    service_status->last_time_unknown = s->get_last_time_unknown();
    service_status->last_time_warning = s->get_last_time_warning();
    service_status->last_update = time(nullptr);
    service_status->latency = s->get_latency();
    service_status->max_check_attempts = s->max_check_attempts();
    service_status->next_check = s->get_next_check();
    service_status->next_notification = s->get_next_notification();
    service_status->no_more_notifications = s->get_no_more_notifications();
    service_status->notifications_enabled = s->get_notifications_enabled();
    service_status->obsess_over = s->obsess_over();
    if (!s->get_plugin_output().empty()) {
      service_status->output =
          misc::string::check_string_utf8(s->get_plugin_output());
      service_status->output.append("\n");
    }
    if (!s->get_long_plugin_output().empty())
      service_status->output.append(
          misc::string::check_string_utf8(s->get_long_plugin_output()));

    service_status->passive_checks_enabled = s->passive_checks_enabled();
    service_status->percent_state_change = s->get_percent_state_change();
    if (!s->get_perf_data().empty())
      service_status->perf_data =
          misc::string::check_string_utf8(s->get_perf_data());
    service_status->retry_interval = s->retry_interval();
    if (s->get_hostname().empty())
      throw msg_fmt("unnamed host");
    if (s->get_description().empty())
      throw msg_fmt("unnamed service");
    service_status->host_name =
        misc::string::check_string_utf8(s->get_hostname());
    service_status->service_description =
        misc::string::check_string_utf8(s->get_description());
    {
      std::pair<uint64_t, uint64_t> p{engine::get_host_and_service_id(
          s->get_hostname(), s->get_description())};
      service_status->host_id = p.first;
      service_status->service_id = p.second;
      if (!service_status->host_id || !service_status->service_id)
        throw msg_fmt("could not find ID of service ('{}', '{}')",
                      service_status->host_name,
                      service_status->service_description);
    }
    service_status->should_be_scheduled = s->get_should_be_scheduled();
    service_status->state_type =
        (s->has_been_checked() ? s->get_state_type() : engine::notifier::hard);

    // Send event(s).
    gl_publisher.write(service_status);

    // Acknowledgement event.
    auto it = gl_acknowledgements.find(
        std::make_pair(service_status->host_id, service_status->service_id));
    if (it != gl_acknowledgements.end() && !service_status->acknowledged) {
      if (!(!service_status->current_state  // !(OK or (normal ack and NOK))
            || (!it->second.is_sticky &&
                (service_status->current_state != it->second.state)))) {
        auto ack{std::make_shared<neb::acknowledgement>(it->second)};
        ack->deletion_time = time(nullptr);
        gl_publisher.write(ack);
      }
      gl_acknowledgements.erase(it);
    }
  } catch (std::exception const& e) {
    log_v2::neb()->error(
        "callbacks: error occurred while generating service status event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 *  Unregister callbacks.
 */
void neb::unregister_callbacks() {
  gl_registered_callbacks.clear();
}
