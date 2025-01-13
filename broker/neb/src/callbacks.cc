/**
 * Copyright 2009-2024 Centreon
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

#include "com/centreon/broker/neb/callbacks.hh"

#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <unistd.h>

#include "com/centreon/broker/bbdo/internal.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/neb/initial.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/set_log_data.hh"
#include "com/centreon/common/file.hh"
#include "com/centreon/common/time.hh"
#include "com/centreon/common/utf8.hh"
#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/nebcallbacks.hh"
#include "com/centreon/engine/nebstructs.hh"
#include "com/centreon/engine/severity.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;

// List of Nagios modules.
extern nebmodule* neb_module_list;

// Acknowledgements list.
static absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                           std::shared_ptr<io::data>>
    gl_acknowledgements;

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
unsigned neb::gl_mod_flags(0);

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
    {NEBCALLBACK_EXTERNAL_COMMAND_DATA, &neb::callback_external_command},
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
    {NEBCALLBACK_ACKNOWLEDGEMENT_DATA, &neb::callback_pb_acknowledgement},
    {NEBCALLBACK_COMMENT_DATA, &neb::callback_pb_comment},
    {NEBCALLBACK_DOWNTIME_DATA, &neb::callback_pb_downtime},
    {NEBCALLBACK_EXTERNAL_COMMAND_DATA, &neb::callback_pb_external_command},
    {NEBCALLBACK_HOST_CHECK_DATA, &neb::callback_pb_host_check},
    {NEBCALLBACK_HOST_STATUS_DATA, &neb::callback_pb_host_status},
    {NEBCALLBACK_PROGRAM_STATUS_DATA, &neb::callback_pb_program_status},
    {NEBCALLBACK_SERVICE_CHECK_DATA, &neb::callback_pb_service_check},
    {NEBCALLBACK_SERVICE_STATUS_DATA, &neb::callback_pb_service_status},
    {NEBCALLBACK_ADAPTIVE_SEVERITY_DATA, &neb::callback_severity},
    {NEBCALLBACK_ADAPTIVE_TAG_DATA, &neb::callback_tag},
    {NEBCALLBACK_OTL_METRICS, &neb::callback_otl_metrics}};

// List of Engine-specific callbacks.
static struct {
  uint32_t macro;
  int (*callback)(int, void*);
} const gl_engine_callbacks[] = {
    {NEBCALLBACK_ADAPTIVE_HOST_DATA, &neb::callback_host},
    {NEBCALLBACK_ADAPTIVE_SERVICE_DATA, &neb::callback_service},
    {NEBCALLBACK_CUSTOM_VARIABLE_DATA, &neb::callback_custom_variable},
    {NEBCALLBACK_GROUP_DATA, &neb::callback_group},
    {NEBCALLBACK_GROUP_MEMBER_DATA, &neb::callback_group_member},
    {NEBCALLBACK_RELATION_DATA, &neb::callback_relation},
    {NEBCALLBACK_BENCH_DATA, &neb::callback_pb_bench},
    {NEBCALLBACK_AGENT_STATS, &neb::callback_agent_stats}};

static struct {
  uint32_t macro;
  int (*callback)(int, void*);
} const gl_pb_engine_callbacks[] = {
    {NEBCALLBACK_ADAPTIVE_HOST_DATA, &neb::callback_pb_host},
    {NEBCALLBACK_ADAPTIVE_SERVICE_DATA, &neb::callback_pb_service},
    {NEBCALLBACK_CUSTOM_VARIABLE_DATA, &neb::callback_pb_custom_variable},
    {NEBCALLBACK_GROUP_DATA, &neb::callback_pb_group},
    {NEBCALLBACK_GROUP_MEMBER_DATA, &neb::callback_pb_group_member},
    {NEBCALLBACK_RELATION_DATA, &neb::callback_pb_relation},
    {NEBCALLBACK_BENCH_DATA, &neb::callback_pb_bench},
    {NEBCALLBACK_AGENT_STATS, &neb::callback_agent_stats}};

// Registered callbacks.
std::list<std::unique_ptr<neb::callback>> neb::gl_registered_callbacks;

// External function to get program version.
extern "C" {
char const* get_program_version();
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
int neb::callback_process(int, void* data) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: process event callback");

  // Input variables.
  nebstruct_process_data const* process_data;
  static time_t start_time;

  // Check process event type.
  process_data = static_cast<nebstruct_process_data*>(data);
  if (NEBTYPE_PROCESS_EVENTLOOPSTART == process_data->type) {
    SPDLOG_LOGGER_DEBUG(neb_logger,
                        "callbacks: generating process start event");

    // Register callbacks.
    SPDLOG_LOGGER_DEBUG(
        neb_logger, "callbacks: registering callbacks for old BBDO version");
    for (uint32_t i(0); i < sizeof(gl_callbacks) / sizeof(*gl_callbacks); ++i)
      gl_registered_callbacks.emplace_back(std::make_unique<callback>(
          gl_callbacks[i].macro, gl_mod_handle, gl_callbacks[i].callback));

    // Register Engine-specific callbacks.
    if (gl_mod_flags & NEBMODULE_ENGINE) {
      // Register engine callbacks.
      SPDLOG_LOGGER_DEBUG(
          neb_logger, "callbacks: registering callbacks for old BBDO version");
      for (uint32_t i = 0;
           i < sizeof(gl_engine_callbacks) / sizeof(*gl_engine_callbacks); ++i)
        gl_registered_callbacks.emplace_back(std::make_unique<callback>(
            gl_engine_callbacks[i].macro, gl_mod_handle,
            gl_engine_callbacks[i].callback));
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
    SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: instance '{}' running {}",
                        instance->name, instance->is_running);

    // Send initial event and then configuration.
    gl_publisher.write(instance);
    send_initial_configuration();
  } else if (NEBTYPE_PROCESS_EVENTLOOPEND == process_data->type) {
    SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating process end event");
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
    SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: instance '{}' running {}",
                        instance->name, instance->is_running);

    // Send event.
    gl_publisher.write(instance);
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
int neb::callback_pb_process(int callback_type, void* data) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: process event callback");
  (void)callback_type;

  // Input variables.
  nebstruct_process_data const* process_data;
  static time_t start_time;

  std::shared_ptr<pb_instance> inst_obj(std::make_shared<pb_instance>());
  Instance& inst(inst_obj->mut_obj());
  inst.set_engine("Centreon Engine");
  inst.set_pid(getpid());
  inst.set_version(get_program_version());

  /* Here we are Engine. The idea is to know if broker is able to handle the
   * evoluated negotiation. The goal is to send the hash of the configuration
   * directory to the broker. */
  auto& engine_config = config::applier::state::instance().engine_config_dir();
  std::error_code ec;
  if (!engine_config.empty() && std::filesystem::exists(engine_config, ec)) {
    inst.set_engine_config_version(common::hash_directory(
        config::applier::state::instance().engine_config_dir(), ec));
  }
  if (ec) {
    SPDLOG_LOGGER_ERROR(
        neb_logger, "callbacks: error while hashing engine configuration: {}",
        ec.message());
  }

  // Check process event type.
  process_data = static_cast<nebstruct_process_data*>(data);
  if (NEBTYPE_PROCESS_EVENTLOOPSTART == process_data->type) {
    SPDLOG_LOGGER_DEBUG(neb_logger,
                        "callbacks: generating process start event");

    // Register callbacks.
    SPDLOG_LOGGER_DEBUG(
        neb_logger, "callbacks: registering callbacks for new BBDO version");
    for (uint32_t i = 0; i < sizeof(gl_pb_callbacks) / sizeof(*gl_pb_callbacks);
         ++i)
      gl_registered_callbacks.emplace_back(
          std::make_unique<callback>(gl_pb_callbacks[i].macro, gl_mod_handle,
                                     gl_pb_callbacks[i].callback));

    // Register Engine-specific callbacks.
    if (gl_mod_flags & NEBMODULE_ENGINE) {
      // Register engine callbacks.
      SPDLOG_LOGGER_DEBUG(
          neb_logger, "callbacks: registering callbacks for new BBDO version");
      for (uint32_t i = 0;
           i < sizeof(gl_pb_engine_callbacks) / sizeof(*gl_pb_engine_callbacks);
           ++i)
        gl_registered_callbacks.emplace_back(std::make_unique<callback>(
            gl_pb_engine_callbacks[i].macro, gl_mod_handle,
            gl_pb_engine_callbacks[i].callback));
    }

    // Output variable.
    inst.set_instance_id(config::applier::state::instance().poller_id());
    inst.set_running(true);
    inst.set_name(config::applier::state::instance().poller_name());
    start_time = time(nullptr);
    inst.set_start_time(start_time);

    // Send initial event and then configuration.
    gl_publisher.write(inst_obj);
    send_initial_pb_configuration();
  } else if (NEBTYPE_PROCESS_EVENTLOOPEND == process_data->type) {
    SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating process end event");
    // Fill output var.
    inst.set_instance_id(config::applier::state::instance().poller_id());
    inst.set_running(false);
    inst.set_name(config::applier::state::instance().poller_name());
    inst.set_end_time(time(nullptr));
    inst.set_start_time(start_time);

    // Send event.
    gl_publisher.write(inst_obj);
  }
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: instance '{}' running {}",
                      inst.name(), inst.running());
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
  SPDLOG_LOGGER_DEBUG(neb_logger,
                      "callbacks: generating instance status event");
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
      is->global_host_event_handler = common::check_string_utf8(
          program_status_data->global_host_event_handler);
    if (!program_status_data->global_service_event_handler.empty())
      is->global_service_event_handler = common::check_string_utf8(
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
int neb::callback_pb_program_status(int, void* data) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger,
                      "callbacks: generating pb instance status event");

  // In/Out variables.
  std::shared_ptr<neb::pb_instance_status> is_obj{
      std::make_shared<neb::pb_instance_status>()};
  InstanceStatus& is(is_obj->mut_obj());

  // Fill output var.
  const nebstruct_program_status_data& program_status_data =
      *static_cast<nebstruct_program_status_data*>(data);

  SPDLOG_LOGGER_DEBUG(neb_logger,
                      "callbacks: generating pb instance status event "
                      "global_service_event_handler={}",
                      program_status_data.global_host_event_handler);

  is.set_instance_id(config::applier::state::instance().poller_id());
  is.set_active_host_checks(program_status_data.active_host_checks_enabled);
  is.set_active_service_checks(
      program_status_data.active_service_checks_enabled);
  is.set_check_hosts_freshness(check_host_freshness);
  is.set_check_services_freshness(check_service_freshness);
  is.set_event_handlers(program_status_data.event_handlers_enabled);
  is.set_flap_detection(program_status_data.flap_detection_enabled);
  if (!program_status_data.global_host_event_handler.empty())
    is.set_global_host_event_handler(common::check_string_utf8(
        program_status_data.global_host_event_handler));
  if (!program_status_data.global_service_event_handler.empty())
    is.set_global_service_event_handler(common::check_string_utf8(
        program_status_data.global_service_event_handler));
  is.set_last_alive(time(nullptr));
  is.set_last_command_check(program_status_data.last_command_check);
  is.set_notifications(program_status_data.notifications_enabled);
  is.set_obsess_over_hosts(program_status_data.obsess_over_hosts);
  is.set_obsess_over_services(program_status_data.obsess_over_services);
  is.set_passive_host_checks(program_status_data.passive_host_checks_enabled);
  is.set_passive_service_checks(
      program_status_data.passive_service_checks_enabled);

  // Send event.
  gl_publisher.write(is_obj);
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
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating relation event");
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
        int host_id = relation->dep_hst->host_id();
        int parent_id = relation->hst->host_id();
        if (host_id && parent_id) {
          // Generate parent event.
          auto new_host_parent{std::make_shared<host_parent>()};
          new_host_parent->enabled = (relation->type != NEBTYPE_PARENT_DELETE);
          new_host_parent->host_id = host_id;
          new_host_parent->parent_id = parent_id;

          // Send event.
          SPDLOG_LOGGER_DEBUG(
              neb_logger, "callbacks: host {} is parent of host {}",
              new_host_parent->parent_id, new_host_parent->host_id);
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
int neb::callback_pb_relation(int callback_type [[maybe_unused]], void* data) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating pb relation event");

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
        int host_id = relation->dep_hst->host_id();
        int parent_id = relation->hst->host_id();
        if (host_id && parent_id) {
          // Generate parent event.
          auto new_host_parent{std::make_shared<pb_host_parent>()};
          new_host_parent->mut_obj().set_enabled(relation->type !=
                                                 NEBTYPE_PARENT_DELETE);
          new_host_parent->mut_obj().set_child_id(host_id);
          new_host_parent->mut_obj().set_parent_id(parent_id);

          // Send event.
          SPDLOG_LOGGER_DEBUG(neb_logger,
                              "callbacks: pb host {} is parent of host {}",
                              parent_id, host_id);
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
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating service check event");
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
          common::check_string_utf8(scdata->command_line);
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
    SPDLOG_LOGGER_ERROR(
        neb_logger,
        "callbacks: error occurred while generating service check event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
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
int neb::callback_pb_service_check(int, void* data) {
  const nebstruct_service_check_data* scdata =
      static_cast<nebstruct_service_check_data*>(data);

  /* For each check, this event is received three times one precheck, one
   * initiate and one processed. We just keep the initiate one. At the
   * processed one we also received the service status. */
  if (scdata->type != NEBTYPE_SERVICECHECK_INITIATE)
    return 0;

  // Log message.
  if (neb_logger->level() <= spdlog::level::debug) {
    SPDLOG_LOGGER_DEBUG(neb_logger,
                        "callbacks: generating service check event host {} "
                        "service {} command_line={}",
                        scdata->host_id, scdata->service_id,
                        scdata->command_line);
  } else {
    SPDLOG_LOGGER_DEBUG(neb_logger,
                        "callbacks: generating service check event");
  }

  // In/Out variables.
  std::shared_ptr<neb::pb_service_check> service_check{
      std::make_shared<neb::pb_service_check>()};
  // Fill output var.
  engine::service* s{static_cast<engine::service*>(scdata->object_ptr)};
  if (scdata->command_line) {
    service_check->mut_obj().set_active_checks_enabled(
        s->active_checks_enabled());
    service_check->mut_obj().set_check_type(
        scdata->check_type ==
                com::centreon::engine::checkable::check_type::check_active
            ? com::centreon::broker::CheckActive
            : com::centreon::broker::CheckPassive);
    service_check->mut_obj().set_command_line(
        common::check_string_utf8(scdata->command_line));
    service_check->mut_obj().set_host_id(scdata->host_id);
    service_check->mut_obj().set_service_id(scdata->service_id);
    service_check->mut_obj().set_next_check(s->get_next_check());

    // Send event.
    gl_publisher.write(service_check);
  }
  return 0;
}

int32_t neb::callback_pb_service_status(int callback_type [[maybe_unused]],
                                        void* data) noexcept {
  SPDLOG_LOGGER_DEBUG(
      neb_logger, "callbacks: generating pb service status check result event");

  nebstruct_service_status_data* ds =
      static_cast<nebstruct_service_status_data*>(data);
  const engine::service* es = static_cast<engine::service*>(ds->object_ptr);
  neb_logger->debug(
      "callbacks: pb_service_status ({},{}) status {}, attributes {}, type {}, "
      "last check {}",
      es->host_id(), es->service_id(),
      static_cast<uint32_t>(es->get_current_state()), ds->attributes,
      static_cast<uint32_t>(es->get_check_type()), es->get_last_check());

  auto handle_acknowledgement = [](uint16_t state, auto& r) {
    neb_logger->debug("Looking for acknowledgement on service ({}:{})",
                      r.host_id(), r.service_id());
    auto it =
        gl_acknowledgements.find(std::make_pair(r.host_id(), r.service_id()));
    if (it != gl_acknowledgements.end() &&
        r.acknowledgement_type() == AckType::NONE) {
      neb_logger->debug("acknowledgement found on service ({}:{})", r.host_id(),
                        r.service_id());
      if (it->second->type() == make_type(io::neb, de_pb_acknowledgement)) {
        neb::pb_acknowledgement* a =
            static_cast<neb::pb_acknowledgement*>(it->second.get());
        if (!(!state  // !(OK or (normal ack and NOK))
              || (!a->obj().sticky() && state != a->obj().state()))) {
          a->mut_obj().set_deletion_time(time(nullptr));
          gl_publisher.write(std::move(it->second));
        }
      } else {
        neb::acknowledgement* a =
            static_cast<neb::acknowledgement*>(it->second.get());
        if (!(!state  // !(OK or (normal ack and NOK))
              || (!a->is_sticky && state != a->state))) {
          a->deletion_time = time(nullptr);
          gl_publisher.write(std::move(it->second));
        }
      }
      gl_acknowledgements.erase(it);
    }
  };
  uint16_t state =
      es->has_been_checked() ? es->get_current_state() : 4;  // Pending state.
  if (ds->attributes != engine::service::STATUS_ALL) {
    auto as = std::make_shared<neb::pb_adaptive_service_status>();
    AdaptiveServiceStatus& asscr = as.get()->mut_obj();
    fill_service_type(asscr, es);
    if (ds->attributes & engine::service::STATUS_DOWNTIME_DEPTH) {
      asscr.set_host_id(es->host_id());
      asscr.set_service_id(es->service_id());
      asscr.set_scheduled_downtime_depth(es->get_scheduled_downtime_depth());
    }
    if (ds->attributes & engine::service::STATUS_NOTIFICATION_NUMBER) {
      asscr.set_host_id(es->host_id());
      asscr.set_service_id(es->service_id());
      asscr.set_notification_number(es->get_notification_number());
    }
    if (ds->attributes & engine::service::STATUS_ACKNOWLEDGEMENT) {
      asscr.set_host_id(es->host_id());
      asscr.set_service_id(es->service_id());
      asscr.set_acknowledgement_type(es->get_acknowledgement());
    }
    gl_publisher.write(as);

    // Acknowledgement event.
    handle_acknowledgement(state, asscr);
  } else {
    auto s{std::make_shared<neb::pb_service_status>()};
    ServiceStatus& sscr = s.get()->mut_obj();

    fill_service_type(sscr, es);
    sscr.set_host_id(es->host_id());
    sscr.set_service_id(es->service_id());
    if (es->host_id() == 0 || es->service_id() == 0)
      SPDLOG_LOGGER_ERROR(neb_logger,
                          "could not find ID of service ('{}', '{}')",
                          es->get_hostname(), es->description());

    sscr.set_acknowledgement_type(es->get_acknowledgement());

    sscr.set_check_type(
        static_cast<ServiceStatus_CheckType>(es->get_check_type()));
    sscr.set_check_attempt(es->get_current_attempt());
    sscr.set_state(static_cast<ServiceStatus_State>(state));
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
      sscr.set_output(common::check_string_utf8(es->get_plugin_output()));
    if (!es->get_long_plugin_output().empty())
      sscr.set_long_output(
          common::check_string_utf8(es->get_long_plugin_output()));
    sscr.set_percent_state_change(es->get_percent_state_change());
    if (!es->get_perf_data().empty()) {
      sscr.set_perfdata(common::check_string_utf8(es->get_perf_data()));
      SPDLOG_LOGGER_TRACE(neb_logger,
                          "callbacks: service ({}, {}) has perfdata <<{}>>",
                          es->host_id(), es->service_id(), es->get_perf_data());
    } else {
      SPDLOG_LOGGER_TRACE(neb_logger,
                          "callbacks: service ({}, {}) has no perfdata",
                          es->host_id(), es->service_id());
    }
    sscr.set_should_be_scheduled(es->get_should_be_scheduled());
    sscr.set_state_type(static_cast<ServiceStatus_StateType>(
        es->has_been_checked() ? es->get_state_type()
                               : engine::notifier::hard));
    sscr.set_scheduled_downtime_depth(es->get_scheduled_downtime_depth());

    // Send event(s).
    gl_publisher.write(s);

    // Acknowledgement event.
    handle_acknowledgement(state, sscr);
  }
  neb_logger->debug("Still {} running acknowledgements",
                    gl_acknowledgements.size());
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
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating service status event");
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
          common::check_string_utf8(s->check_command());
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
          common::check_string_utf8(s->event_handler());
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
          common::check_string_utf8(s->get_plugin_output());
      service_status->output.append("\n");
    }
    if (!s->get_long_plugin_output().empty())
      service_status->output.append(
          common::check_string_utf8(s->get_long_plugin_output()));

    service_status->passive_checks_enabled = s->passive_checks_enabled();
    service_status->percent_state_change = s->get_percent_state_change();
    if (!s->get_perf_data().empty())
      service_status->perf_data = common::check_string_utf8(s->get_perf_data());
    service_status->retry_interval = s->retry_interval();
    if (s->get_hostname().empty())
      throw msg_fmt("unnamed host");
    if (s->description().empty())
      throw msg_fmt("unnamed service");
    service_status->host_name = common::check_string_utf8(s->get_hostname());
    service_status->service_description =
        common::check_string_utf8(s->description());
    {
      std::pair<uint64_t, uint64_t> p{
          engine::get_host_and_service_id(s->get_hostname(), s->description())};
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
      neb_logger->debug("acknowledgement found on service ({}:{})",
                        service_status->host_id, service_status->service_id);
      if (it->second->type() == make_type(io::neb, de_pb_acknowledgement)) {
        neb::pb_acknowledgement* a =
            static_cast<neb::pb_acknowledgement*>(it->second.get());
        if (!(!service_status->current_state  // !(OK or (normal ack and NOK))
              || (!a->obj().sticky() &&
                  service_status->current_state !=
                      static_cast<short>(a->obj().state())))) {
          a->mut_obj().set_deletion_time(time(nullptr));
          gl_publisher.write(std::move(it->second));
        }
      } else {
        neb::acknowledgement* a =
            static_cast<neb::acknowledgement*>(it->second.get());
        if (!(!service_status->current_state  // !(OK or (normal ack and NOK))
              ||
              (!a->is_sticky && service_status->current_state != a->state))) {
          a->deletion_time = time(nullptr);
          gl_publisher.write(std::move(it->second));
        }
      }
      gl_acknowledgements.erase(it);
    }
    neb_logger->debug("Still {} running acknowledgements",
                      gl_acknowledgements.size());
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(
        neb_logger,
        "callbacks: error occurred while generating service status event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
  return 0;
}

/**
 * @brief generate a bench event that will collect timepoints at each muxer
 * traversal
 *
 * @param data
 * @return int
 */
int neb::callback_pb_bench(int, void* data) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating pb_bench event");
  const nebstruct_bench_data* bench_data =
      static_cast<nebstruct_bench_data*>(data);

  std::shared_ptr<bbdo::pb_bench> event = std::make_shared<bbdo::pb_bench>();
  event->mut_obj().set_id(bench_data->id);
  if (bench_data->mess_create != std::chrono::system_clock::time_point::min()) {
    TimePoint* caller_tp = event->mut_obj().add_points();
    caller_tp->set_name("client");
    caller_tp->set_function("callback_pb_bench");
    common::time_point_to_google_ts(bench_data->mess_create,
                                    *caller_tp->mutable_time());
  }
  gl_publisher.write(std::move(event));
  return 0;
}

namespace com::centreon::broker::neb::otl_detail {
/**
 * @brief the goal of this little class is to avoid copy of an
 * ExportMetricsServiceRequest as callback_otl_metrics receives a
 * shared_ptr<ExportMetricsServiceRequest>
 *
 */
class otl_protobuf
    : public io::protobuf<opentelemetry::proto::collector::metrics::v1::
                              ExportMetricsServiceRequest,
                          make_type(io::storage, storage::de_pb_otl_metrics)> {
  std::shared_ptr<
      opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest>
      _obj;

 public:
  otl_protobuf(void* pointer_to_shared_ptr)
      : _obj(*static_cast<
             std::shared_ptr<opentelemetry::proto::collector::metrics::v1::
                                 ExportMetricsServiceRequest>*>(
            pointer_to_shared_ptr)) {}

  const opentelemetry::proto::collector::metrics::v1::
      ExportMetricsServiceRequest&
      obj() const override {
    return *_obj;
  }

  opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest&
  mut_obj() override {
    return *_obj;
  }

  void set_obj(opentelemetry::proto::collector::metrics::v1::
                   ExportMetricsServiceRequest&& obj
               [[maybe_unused]]) override {
    throw com::centreon::exceptions::msg_fmt("unauthorized usage {}",
                                             typeid(*this).name());
  }
};

}  // namespace com::centreon::broker::neb::otl_detail

/**
 * @brief send an ExportMetricsServiceRequest to broker
 *
 * @param data pointer to a shared_ptr<ExportMetricsServiceRequest>
 * @return int 0
 */
int neb::callback_otl_metrics(int, void* data) {
  gl_publisher.write(std::make_shared<neb::otl_detail::otl_protobuf>(data));
  return 0;
}

int neb::callback_agent_stats(int, void* data) {
  nebstruct_agent_stats_data* ds =
      static_cast<nebstruct_agent_stats_data*>(data);

  auto to_send = std::make_shared<neb::pb_agent_stats>();

  to_send->mut_obj().set_poller_id(
      config::applier::state::instance().poller_id());

  for (const auto& cumul_data : *ds->data) {
    AgentInfo* to_fill = to_send->mut_obj().add_stats();
    to_fill->set_major(cumul_data.major);
    to_fill->set_minor(cumul_data.minor);
    to_fill->set_patch(cumul_data.patch);
    to_fill->set_reverse(cumul_data.reverse);
    to_fill->set_os(cumul_data.os);
    to_fill->set_os_version(cumul_data.os_version);
    to_fill->set_nb_agent(cumul_data.nb_agent);
  }

  gl_publisher.write(to_send);
  return 0;
}

/**
 *  Unregister callbacks.
 */
void neb::unregister_callbacks() {
  gl_registered_callbacks.clear();
}
