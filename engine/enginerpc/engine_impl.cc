/**
 * Copyright 2022-2024 Centreon
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

#include <google/protobuf/util/time_util.h>
#include <sys/types.h>
#include <future>

#include <boost/asio.hpp>

#include <absl/strings/str_join.h>

#include <spdlog/common.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "com/centreon/common/process_stat.hh"
#include "com/centreon/common/time.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/commands/connector.hh"
#include "com/centreon/engine/commands/processing.hh"
#include "com/centreon/engine/downtimes/downtime_finder.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/hostescalation.hh"
#include "com/centreon/engine/serviceescalation.hh"
#include "com/centreon/engine/severity.hh"
#include "com/centreon/engine/statusdata.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/engine/version.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::string;

using com::centreon::common::log_v2::log_v2;

namespace com::centreon::engine {

std::ostream& operator<<(std::ostream& str, const NameOrIdIdentifier& host_id) {
  switch (host_id.identifier_case()) {
    case NameOrIdIdentifier::kName:
      str << "host name=" << host_id.name();
      break;
    case NameOrIdIdentifier::kId:
      str << "host id=" << host_id.id();
      break;
    default:
      str << " host nor id nor name";
  }
  return str;
}

std::ostream& operator<<(std::ostream& str, const ServiceIdentifier& serv_id) {
  switch (serv_id.identifier_case()) {
    case ServiceIdentifier::kNames:
      str << "host name=" << serv_id.names().host_name()
          << " serv name=" << serv_id.names().service_name();
      break;
    case ServiceIdentifier::kIds:
      str << "host id=" << serv_id.ids().host_id()
          << " serv id=" << serv_id.ids().service_id();
      break;
    default:
      str << " serv nor id nor name";
  }
  return str;
}

}  // namespace com::centreon::engine

namespace fmt {
template <>
struct formatter<NameOrIdIdentifier> : ostream_formatter {};
template <>
struct formatter<ServiceIdentifier> : ostream_formatter {};

}  // namespace fmt

/**
 * @brief Return the Engine's version.
 *
 * @param context gRPC context
 * @param  unused
 * @param response A Version object to fill
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetVersion(
    grpc::ServerContext* /*context*/,
    const ::google::protobuf::Empty* /*request*/,
    Version* response) {
  response->set_major(CENTREON_ENGINE_VERSION_MAJOR);
  response->set_minor(CENTREON_ENGINE_VERSION_MINOR);
  response->set_patch(CENTREON_ENGINE_VERSION_PATCH);
  return grpc::Status::OK;
}

grpc::Status engine_impl::GetStats(grpc::ServerContext* context
                                   [[maybe_unused]],
                                   const GenericString* request
                                   [[maybe_unused]],
                                   Stats* response) {
  auto fn = std::packaged_task<int(void)>(
      std::bind(&command_manager::get_stats, &command_manager::instance(),
                request->str_arg(), response));
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));
  int32_t res = result.get();
  if (res == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::StatusCode::UNKNOWN, "Unknown error");
}

grpc::Status engine_impl::ProcessServiceCheckResult(grpc::ServerContext* context
                                                    [[maybe_unused]],
                                                    const Check* request,
                                                    CommandSuccess* response
                                                    [[maybe_unused]]) {
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  std::string const& svc_desc = request->svc_desc();
  if (svc_desc.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "svc_desc must not be empty");

  auto fn = std::packaged_task<int(void)>(
      std::bind(&command_manager::process_passive_service_check,
                &command_manager::instance(),
                google::protobuf::util::TimeUtil::TimestampToSeconds(
                    request->check_time()),
                host_name, svc_desc, request->code(), request->output()));
  command_manager::instance().enqueue(std::move(fn));

  return grpc::Status::OK;
}

grpc::Status engine_impl::ProcessHostCheckResult(grpc::ServerContext* context
                                                 [[maybe_unused]],
                                                 const Check* request,
                                                 CommandSuccess* response
                                                 [[maybe_unused]]) {
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  auto fn = std::packaged_task<int(void)>(
      std::bind(&command_manager::process_passive_host_check,
                &command_manager::instance(),
                google::protobuf::util::TimeUtil::TimestampToSeconds(
                    request->check_time()),
                host_name, request->code(), request->output()));
  command_manager::instance().enqueue(std::move(fn));

  return grpc::Status::OK;
}

/**
 * @brief When a new file arrives on the centreon server, this command is used
 * to notify engine to update its anomaly detection services with those new
 * thresholds. The update is done by the main loop thread because of concurrent
 * accesses.
 *
 * @param
 * @param request The Protobuf message containing the full name of the
 *                thresholds file.
 * @param
 *
 * @return A grpc::Status::OK if the file is well read, an error otherwise.
 */
grpc::Status engine_impl::NewThresholdsFile(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const ThresholdsFile* request,
                                            CommandSuccess* response
                                            [[maybe_unused]]) {
  const std::string& filename = request->filename();
  auto fn = std::packaged_task<int(void)>(
      std::bind(&anomalydetection::update_thresholds, filename));
  command_manager::instance().enqueue(std::move(fn));
  return grpc::Status::OK;
}

/**
 * @brief Return host informations.
 *
 * @param context gRPC context
 * @param request Host's identifier (it can be a hostname or a hostid)
 * @param response The filled fields
 *
 * @return Status::OK if the Host is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 */
grpc::Status engine_impl::GetHost(grpc::ServerContext* context [[maybe_unused]],
                                  const NameOrIdIdentifier* request
                                  [[maybe_unused]],
                                  EngineHost* response) {
  std::string err;

  auto fn = std::packaged_task<int(void)>([&err, request,
                                           host = response]() -> int32_t {
    std::shared_ptr<com::centreon::engine::host> selectedhost;
    std::tie(selectedhost, err) = get_host(*request);

    if (!err.empty()) {
      return 1;
    }

    host->set_name(selectedhost->name());
    host->set_alias(selectedhost->get_alias());
    host->set_address(selectedhost->get_address());
    host->set_check_period(selectedhost->check_period());
    host->set_id(selectedhost->host_id());
    host->set_current_state(
        static_cast<EngineHost::State>(selectedhost->get_current_state()));
    host->set_display_name(selectedhost->get_display_name());

    if (!selectedhost->parent_hosts.empty())
      for (const auto& [key, _] : selectedhost->parent_hosts)
        host->add_parent_hosts(key);

    if (!selectedhost->child_hosts.empty())
      for (const auto& [key, _] : selectedhost->child_hosts)
        host->add_child_hosts(key);

    if (!selectedhost->services.empty())
      for (const auto& [key, _] : selectedhost->services)
        host->add_services(fmt::format("{},{}", key.first, key.second));

    host->set_check_command(selectedhost->check_command());
    host->set_initial_state(
        static_cast<EngineHost::State>(selectedhost->get_initial_state()));
    host->set_check_interval(selectedhost->check_interval());
    host->set_retry_interval(selectedhost->retry_interval());
    host->set_max_attempts(selectedhost->max_check_attempts());
    host->set_event_handler(selectedhost->event_handler());

    if (!selectedhost->get_contactgroups().empty())
      for (const auto& [key, _] : selectedhost->get_contactgroups())
        host->add_contactgroups(key);

    if (!selectedhost->contacts().empty())
      for (const auto& [key, _] : selectedhost->contacts())
        host->add_contacts(key);

    host->set_notification_interval(selectedhost->get_notification_interval());
    host->set_first_notification_delay(
        selectedhost->get_first_notification_delay());
    host->set_recovery_notification_delay(
        selectedhost->get_recovery_notification_delay());
    host->set_notify_up(selectedhost->get_notify_on(notifier::up));
    host->set_notify_down(selectedhost->get_notify_on(notifier::down));
    host->set_notify_unreachable(
        selectedhost->get_notify_on(notifier::unreachable));
    host->set_notify_on_flappingstart(
        selectedhost->get_notify_on(notifier::flappingstart));
    host->set_notify_on_flappingstop(
        selectedhost->get_notify_on(notifier::flappingstop));
    host->set_notify_on_flappingdisabled(
        selectedhost->get_notify_on(notifier::flappingdisabled));
    host->set_notify_downtime(selectedhost->get_notify_on(notifier::downtime));
    host->set_notification_period(selectedhost->notification_period());
    host->set_flap_detection_enabled(selectedhost->flap_detection_enabled());
    host->set_low_flap_threshold(selectedhost->get_low_flap_threshold());
    host->set_high_flap_threshold(selectedhost->get_high_flap_threshold());
    host->set_flap_detection_on_up(
        selectedhost->get_flap_detection_on(notifier::up));
    host->set_flap_detection_on_down(
        selectedhost->get_flap_detection_on(notifier::down));
    host->set_flap_detection_on_unreachable(
        selectedhost->get_flap_detection_on(notifier::unreachable));
    host->set_stalk_on_up(selectedhost->get_stalk_on(notifier::up));
    host->set_stalk_on_down(selectedhost->get_stalk_on(notifier::down));
    host->set_stalk_on_unreachable(
        selectedhost->get_stalk_on(notifier::unreachable));
    host->set_check_freshness(selectedhost->check_freshness_enabled());
    host->set_freshness_threshold(selectedhost->get_freshness_threshold());
    host->set_process_performance_data(
        selectedhost->get_process_performance_data());
    host->set_checks_enabled(selectedhost->active_checks_enabled());
    host->set_accept_passive_checks(selectedhost->passive_checks_enabled());
    host->set_event_handler_enabled(selectedhost->event_handler_enabled());
    host->set_retain_status_information(
        selectedhost->get_retain_status_information());
    host->set_retain_nonstatus_information(
        selectedhost->get_retain_nonstatus_information());
    host->set_obsess_over_host(selectedhost->obsess_over());
    host->set_notes(selectedhost->get_notes());
    host->set_notes_url(selectedhost->get_notes_url());
    host->set_action_url(selectedhost->get_action_url());
    host->set_icon_image(selectedhost->get_icon_image());
    host->set_icon_image_alt(selectedhost->get_icon_image_alt());
    host->set_vrml_image(selectedhost->get_vrml_image());
    host->set_statusmap_image(selectedhost->get_statusmap_image());
    host->set_have_2d_coords(selectedhost->get_have_2d_coords());
    host->set_x_2d(selectedhost->get_x_2d());
    host->set_y_2d(selectedhost->get_y_2d());
    host->set_have_3d_coords(selectedhost->get_have_3d_coords());
    host->set_x_3d(selectedhost->get_x_3d());
    host->set_y_3d(selectedhost->get_y_3d());
    host->set_z_3d(selectedhost->get_z_3d());
    host->set_should_be_drawn(selectedhost->get_should_be_drawn());
    host->set_acknowledgement(
        static_cast<EngineHost_AckType>(selectedhost->get_acknowledgement()));
    host->set_check_type(
        static_cast<EngineHost_CheckType>(selectedhost->get_check_type()));
    host->set_last_state(
        static_cast<EngineHost_State>(selectedhost->get_last_state()));
    host->set_last_hard_state(
        static_cast<EngineHost_State>(selectedhost->get_last_hard_state()));
    host->set_plugin_output(selectedhost->get_plugin_output());
    host->set_long_plugin_output(selectedhost->get_long_plugin_output());
    host->set_perf_data(selectedhost->get_perf_data());
    host->set_state_type(
        static_cast<EngineHost_State>(selectedhost->get_state_type()));
    host->set_current_attempt(selectedhost->get_current_attempt());
    host->set_current_event_id(selectedhost->get_current_event_id());
    host->set_last_event_id(selectedhost->get_last_event_id());
    host->set_current_problem_id(selectedhost->get_current_problem_id());
    host->set_last_problem_id(selectedhost->get_last_problem_id());
    host->set_latency(selectedhost->get_latency());
    host->set_execution_time(selectedhost->get_execution_time());
    host->set_is_executing(selectedhost->get_is_executing());
    host->set_check_options(selectedhost->get_check_options());
    host->set_notifications_enabled(selectedhost->get_notifications_enabled());
    host->set_last_notification(
        string::ctime(selectedhost->get_last_notification()));
    host->set_next_notification(
        string::ctime(selectedhost->get_next_notification()));
    host->set_next_check(string::ctime(selectedhost->get_next_check()));
    host->set_should_be_scheduled(selectedhost->get_should_be_scheduled());
    host->set_last_check(string::ctime(selectedhost->get_last_check()));
    host->set_last_state_change(
        string::ctime(selectedhost->get_last_state_change()));
    host->set_last_hard_state_change(
        string::ctime(selectedhost->get_last_hard_state_change()));
    host->set_last_time_up(string::ctime(selectedhost->get_last_time_up()));
    host->set_last_time_down(string::ctime(selectedhost->get_last_time_down()));
    host->set_last_time_unreachable(
        string::ctime(selectedhost->get_last_time_unreachable()));
    host->set_has_been_checked(selectedhost->has_been_checked());
    host->set_is_being_freshened(selectedhost->get_is_being_freshened());
    host->set_notified_on_down(selectedhost->get_notified_on(notifier::down));
    host->set_notified_on_unreachable(
        selectedhost->get_notified_on(notifier::unreachable));
    host->set_no_more_notifications(selectedhost->get_no_more_notifications());
    host->set_current_notification_id(
        selectedhost->get_current_notification_id());
    host->set_scheduled_downtime_depth(
        selectedhost->get_scheduled_downtime_depth());
    host->set_pending_flex_downtime(selectedhost->get_pending_flex_downtime());

    host->set_state_history(fmt::format(
        "[{}]", fmt::join(selectedhost->get_state_history(), ", ")));

    host->set_state_history_index(selectedhost->get_state_history_index());
    host->set_last_state_history_update(
        string::ctime(selectedhost->get_last_state_history_update()));
    host->set_is_flapping(selectedhost->get_is_flapping());
    host->set_flapping_comment_id(selectedhost->get_flapping_comment_id());
    host->set_percent_state_change(selectedhost->get_percent_state_change());
    host->set_total_services(selectedhost->get_total_services());
    host->set_total_service_check_interval(
        selectedhost->get_total_service_check_interval());
    host->set_modified_attributes(selectedhost->get_modified_attributes());
    host->set_circular_path_checked(selectedhost->get_circular_path_checked());
    host->set_contains_circular_path(
        selectedhost->get_contains_circular_path());
    host->set_timezone(selectedhost->get_timezone());
    host->set_icon_id(selectedhost->get_icon_id());

    // locals
    for (const auto& hg : selectedhost->get_parent_groups())
      if (hg)
        host->add_group_name(hg->get_group_name());

    host->set_acknowledgement_timeout(selectedhost->acknowledgement_timeout());

    const auto& host_severity = selectedhost->get_severity();

    if (host_severity) {
      host->set_severity_level(host_severity->level());
      host->set_severity_id(host_severity->id());
    }

    if (!selectedhost->tags().empty())
      for (const auto& tag : selectedhost->tags())
        host->add_tag(fmt::format("id:{},name:{},type:{}", tag->id(),
                                  tag->name(), static_cast<int>(tag->type())));

    for (const auto& cv : selectedhost->custom_variables)
      host->add_custom_variables(fmt::format(
          "key : {}, value :{}, is_sent :{}, has_been_modified: {} ", cv.first,
          cv.second.value(), cv.second.is_sent(),
          cv.second.has_been_modified()));

    return 0;
  });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));
  int32_t res = result.get();
  if (res == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return contact informations.
 *
 * @param context gRPC context
 * @param request Contact's identifier
 * @param response The filled fields
 *
 * @return Status::OK if the Contact is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 **/
grpc::Status engine_impl::GetContact(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const NameIdentifier* request,
                                     EngineContact* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>([&err, request,
                                           contact = response]() -> int32_t {
    std::shared_ptr<com::centreon::engine::contact> selectedcontact;
    /* get the contact by his name */
    auto itcontactname = contact::contacts.find(request->name());
    if (itcontactname != contact::contacts.end())
      selectedcontact = itcontactname->second;
    else {
      err = fmt::format("could not find contact '{}'", request->name());
      return 1;
    }
    /* recovering contact's information */
    contact->set_name(selectedcontact->get_name());
    contact->set_alias(selectedcontact->get_alias());
    contact->set_email(selectedcontact->get_email());

    if (!selectedcontact->get_parent_groups().empty())
      for (const auto& [key, _] : selectedcontact->get_parent_groups())
        contact->add_contact_groups(key);

    contact->set_pager(selectedcontact->get_pager());
    contact->set_host_notification_period(
        selectedcontact->get_host_notification_period());

    if (!selectedcontact->get_host_notification_commands().empty()) {
      for (const auto& cmd : selectedcontact->get_host_notification_commands())
        contact->add_host_notification_commands(cmd->get_name());
    }
    contact->set_service_notification_period(
        selectedcontact->get_service_notification_period());

    if (!selectedcontact->get_service_notification_commands().empty()) {
      for (const auto& cmd :
           selectedcontact->get_service_notification_commands())
        contact->add_service_notification_commands(cmd->get_name());
    }

    contact->set_host_notification_on_up(
        selectedcontact->notify_on(notifier::host_notification, notifier::up));
    contact->set_host_notification_on_down(selectedcontact->notify_on(
        notifier::host_notification, notifier::down));
    contact->set_host_notification_on_unreachable(selectedcontact->notify_on(
        notifier::host_notification, notifier::unreachable));
    contact->set_host_notification_on_flappingstart(selectedcontact->notify_on(
        notifier::host_notification, notifier::flappingstart));
    contact->set_host_notification_on_flappingstop(selectedcontact->notify_on(
        notifier::host_notification, notifier::flappingstop));
    contact->set_host_notification_on_flappingdisabled(
        selectedcontact->notify_on(notifier::host_notification,
                                   notifier::flappingdisabled));
    contact->set_host_notification_on_downtime(selectedcontact->notify_on(
        notifier::host_notification, notifier::downtime));

    contact->set_service_notification_on_ok(selectedcontact->notify_on(
        notifier::service_notification, notifier::ok));
    contact->set_service_notification_on_warning(selectedcontact->notify_on(
        notifier::service_notification, notifier::warning));
    contact->set_service_notification_on_unknown(selectedcontact->notify_on(
        notifier::service_notification, notifier::unknown));
    contact->set_service_notification_on_critical(selectedcontact->notify_on(
        notifier::service_notification, notifier::critical));
    contact->set_service_notification_on_flappingstart(
        selectedcontact->notify_on(notifier::service_notification,
                                   notifier::flappingstart));
    contact->set_service_notification_on_flappingstop(
        selectedcontact->notify_on(notifier::service_notification,
                                   notifier::flappingstop));
    contact->set_service_notification_on_flappingdisabled(
        selectedcontact->notify_on(notifier::service_notification,
                                   notifier::flappingdisabled));
    contact->set_service_notification_on_downtime(selectedcontact->notify_on(
        notifier::service_notification, notifier::downtime));

    contact->set_host_notifications_enabled(
        selectedcontact->get_host_notifications_enabled());
    contact->set_service_notifications_enabled(
        selectedcontact->get_service_notifications_enabled());
    contact->set_can_submit_commands(
        selectedcontact->get_can_submit_commands());
    contact->set_retain_status_information(
        selectedcontact->get_retain_status_information());
    contact->set_retain_nonstatus_information(
        selectedcontact->get_retain_nonstatus_information());
    contact->set_timezone(selectedcontact->get_timezone());

    if (!selectedcontact->get_addresses().empty())
      for (const auto& addr : selectedcontact->get_addresses())
        contact->add_addresses(addr);

    for (const auto& [key, custom_variable] :
         selectedcontact->get_custom_variables())
      contact->add_custom_variables(fmt::format(
          "key : {}, value :{}, is_sent :{}, has_been_modified: {} ", key,
          custom_variable.value(), custom_variable.is_sent(),
          custom_variable.has_been_modified()));

    return 0;
  });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));
  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return service informations.
 *
 * @param context gRPC context
 * @param request Service's identifier (it can be a hostname & servicename or a
 *        hostid & serviceid)
 * @param response The filled fields
 *
 * @return Status::OK if the Service is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 */
grpc::Status engine_impl::GetService(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const ServiceIdentifier* request,
                                     EngineService* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>([&err, request,
                                           service = response]() -> int32_t {
    std::shared_ptr<com::centreon::engine::service> selectedservice;
    std::tie(selectedservice, err) = get_serv(*request);
    if (!err.empty()) {
      return 1;
    }
    /* recovering service's information */
    service->set_host_id(selectedservice->host_id());
    service->set_service_id(selectedservice->service_id());
    service->set_host_name(selectedservice->get_hostname());
    service->set_description(selectedservice->description());
    service->set_check_period(selectedservice->check_period());
    service->set_current_state(static_cast<EngineService::State>(
        selectedservice->get_current_state()));
    service->set_display_name(selectedservice->get_display_name());
    service->set_check_command(selectedservice->check_command());
    service->set_event_handler(selectedservice->event_handler());
    service->set_initial_state(static_cast<EngineService::State>(
        selectedservice->get_initial_state()));
    service->set_check_interval(selectedservice->check_interval());
    service->set_retry_interval(selectedservice->retry_interval());
    service->set_max_check_attempts(selectedservice->max_check_attempts());
    service->set_acknowledgement_timeout(
        selectedservice->acknowledgement_timeout());

    if (!selectedservice->get_contactgroups().empty())
      for (const auto& [key, _] : selectedservice->get_contactgroups())
        service->add_contactgroups(key);

    if (!selectedservice->contacts().empty())
      for (const auto& [key, _] : selectedservice->contacts())
        service->add_contacts(key);

    if (!selectedservice->get_parent_groups().empty())
      for (const auto& grp : selectedservice->get_parent_groups())
        if (grp)
          service->add_servicegroups(grp->get_group_name());

    service->set_notification_interval(
        selectedservice->get_notification_interval());
    service->set_first_notification_delay(
        selectedservice->get_first_notification_delay());
    service->set_recovery_notification_delay(
        selectedservice->get_recovery_notification_delay());
    service->set_notify_on_unknown(
        selectedservice->get_notify_on(notifier::unknown));
    service->set_notify_on_warning(
        selectedservice->get_notify_on(notifier::warning));
    service->set_notify_on_critical(
        selectedservice->get_notify_on(notifier::critical));
    service->set_notify_on_ok(selectedservice->get_notify_on(notifier::ok));
    service->set_notify_on_flappingstart(
        selectedservice->get_notify_on(notifier::flappingstart));
    service->set_notify_on_flappingstop(
        selectedservice->get_notify_on(notifier::flappingstop));
    service->set_notify_on_flappingdisabled(
        selectedservice->get_notify_on(notifier::flappingdisabled));
    service->set_notify_on_downtime(
        selectedservice->get_notify_on(notifier::downtime));
    service->set_stalk_on_ok(selectedservice->get_stalk_on(notifier::ok));
    service->set_stalk_on_warning(
        selectedservice->get_stalk_on(notifier::warning));
    service->set_stalk_on_unknown(
        selectedservice->get_stalk_on(notifier::unknown));
    service->set_stalk_on_critical(
        selectedservice->get_stalk_on(notifier::critical));
    service->set_is_volatile(selectedservice->get_is_volatile());
    service->set_notification_period(selectedservice->notification_period());
    service->set_flap_detection_enabled(
        selectedservice->flap_detection_enabled());
    service->set_low_flap_threshold(selectedservice->get_low_flap_threshold());
    service->set_high_flap_threshold(
        selectedservice->get_high_flap_threshold());
    service->set_flap_detection_on_ok(
        selectedservice->get_flap_detection_on(notifier::ok));
    service->set_flap_detection_on_warning(
        selectedservice->get_flap_detection_on(notifier::warning));
    service->set_flap_detection_on_unknown(
        selectedservice->get_flap_detection_on(notifier::unknown));
    service->set_flap_detection_on_critical(
        selectedservice->get_flap_detection_on(notifier::critical));
    service->set_process_performance_data(
        selectedservice->get_process_performance_data());
    service->set_check_freshness_enabled(
        selectedservice->check_freshness_enabled());
    service->set_freshness_threshold(
        selectedservice->get_freshness_threshold());
    service->set_passive_checks_enabled(
        selectedservice->passive_checks_enabled());
    service->set_event_handler_enabled(
        selectedservice->event_handler_enabled());
    service->set_active_checks_enabled(
        selectedservice->active_checks_enabled());
    service->set_retain_status_information(
        selectedservice->get_retain_status_information());
    service->set_retain_nonstatus_information(
        selectedservice->get_retain_nonstatus_information());
    service->set_notifications_enabled(
        selectedservice->get_notifications_enabled());
    service->set_obsess_over(selectedservice->obsess_over());
    service->set_notes(selectedservice->get_notes());
    service->set_notes_url(selectedservice->get_notes_url());
    service->set_action_url(selectedservice->get_action_url());
    service->set_icon_image(selectedservice->get_icon_image());
    service->set_icon_image_alt(selectedservice->get_icon_image_alt());
    service->set_acknowledgement(static_cast<EngineService::AckType>(
        selectedservice->get_acknowledgement()));
    service->set_host_problem_at_last_check(
        selectedservice->get_host_problem_at_last_check());
    service->set_check_type(static_cast<EngineService::CheckType>(
        selectedservice->get_check_type()));
    service->set_last_state(
        static_cast<EngineService::State>(selectedservice->get_last_state()));
    service->set_last_hard_state(static_cast<EngineService::State>(
        selectedservice->get_last_hard_state()));
    service->set_plugin_output(selectedservice->get_plugin_output());
    service->set_long_plugin_output(selectedservice->get_long_plugin_output());
    service->set_perf_data(selectedservice->get_perf_data());
    service->set_state_type(
        static_cast<EngineService::State>(selectedservice->get_state_type()));
    service->set_next_check(string::ctime(selectedservice->get_next_check()));
    service->set_should_be_scheduled(
        selectedservice->get_should_be_scheduled());
    service->set_last_check(string::ctime(selectedservice->get_last_check()));
    service->set_current_attempt(selectedservice->get_current_attempt());
    service->set_current_event_id(selectedservice->get_current_event_id());
    service->set_last_event_id(selectedservice->get_last_event_id());
    service->set_current_problem_id(selectedservice->get_current_problem_id());
    service->set_last_problem_id(selectedservice->get_last_problem_id());
    service->set_last_notification(
        string::ctime(selectedservice->get_last_notification()));
    service->set_next_notification(
        string::ctime(selectedservice->get_next_notification()));
    service->set_no_more_notifications(
        selectedservice->get_no_more_notifications());
    service->set_last_state_change(
        string::ctime(selectedservice->get_last_state_change()));
    service->set_last_hard_state_change(
        string::ctime(selectedservice->get_last_hard_state_change()));
    service->set_last_time_ok(
        string::ctime(selectedservice->get_last_time_ok()));
    service->set_last_time_warning(
        string::ctime(selectedservice->get_last_time_warning()));
    service->set_last_time_unknown(
        string::ctime(selectedservice->get_last_time_unknown()));
    service->set_last_time_critical(
        string::ctime(selectedservice->get_last_time_critical()));
    service->set_has_been_checked(selectedservice->has_been_checked());
    service->set_is_being_freshened(selectedservice->get_is_being_freshened());
    service->set_notified_on_unknown(
        selectedservice->get_notified_on(notifier::unknown));
    service->set_notified_on_warning(
        selectedservice->get_notified_on(notifier::warning));
    service->set_notified_on_critical(
        selectedservice->get_notified_on(notifier::critical));
    service->set_notification_number(
        selectedservice->get_notification_number());
    service->set_current_notification_id(
        selectedservice->get_current_notification_id());
    service->set_latency(selectedservice->get_latency());
    service->set_execution_time(selectedservice->get_execution_time());
    service->set_is_executing(selectedservice->get_is_executing());
    service->set_check_options(selectedservice->get_check_options());
    service->set_scheduled_downtime_depth(
        selectedservice->get_scheduled_downtime_depth());
    service->set_pending_flex_downtime(
        selectedservice->get_pending_flex_downtime());
    service->set_state_history(fmt::format(
        "[{}]", fmt::join(selectedservice->get_state_history(), ", ")));
    service->set_state_history_index(
        selectedservice->get_state_history_index());
    service->set_is_flapping(selectedservice->get_is_flapping());
    service->set_flapping_comment_id(
        selectedservice->get_flapping_comment_id());
    service->set_percent_state_change(
        selectedservice->get_percent_state_change());
    service->set_modified_attributes(
        selectedservice->get_modified_attributes());
    service->set_host_ptr(selectedservice->get_host_ptr()
                              ? selectedservice->get_host_ptr()->name()
                              : "");
    service->set_event_handler_args(selectedservice->get_event_handler_args());
    service->set_check_command_args(selectedservice->get_check_command_args());
    service->set_timezone(selectedservice->get_timezone());
    service->set_icon_id(selectedservice->get_icon_id());

    const auto& service_severity = selectedservice->get_severity();

    if (service_severity) {
      service->set_severity_level(service_severity->level());
      service->set_severity_id(service_severity->id());
    }

    if (!selectedservice->tags().empty())
      for (const auto& tag : selectedservice->tags())
        service->add_tag(fmt::format("id:{},name:{},type:{}", tag->id(),
                                     tag->name(),
                                     static_cast<int>(tag->type())));

    for (auto const& cv : selectedservice->custom_variables)
      service->add_custom_variables(fmt::format(
          "key : {}, value :{}, is_sent :{}, has_been_modified: {} ", cv.first,
          cv.second.value(), cv.second.is_sent(),
          cv.second.has_been_modified()));

    service->set_service_type(static_cast<EngineService::ServiceType>(
        selectedservice->get_service_type() + 1));

    // if anomaly detection , set the anomaly detection fields
    if (selectedservice->get_service_type() ==
        service_type::ANOMALY_DETECTION) {
      auto selectedanomaly =
          std::static_pointer_cast<com::centreon::engine::anomalydetection>(
              selectedservice);

      service->set_internal_id(selectedanomaly->get_internal_id());
      service->set_metric_name(selectedanomaly->get_metric_name());
      service->set_thresholds_file(selectedanomaly->get_thresholds_file());
      service->set_sensitivity(selectedanomaly->get_sensitivity());
      service->set_dependent_service_id(
          selectedanomaly->get_dependent_service()->service_id());
    }

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return hostgroup informations.
 *
 * @param context gRPC context
 * @param request hostgroup's identifier (it can be a hostgroupename or
 * hostgroupid)
 * @param response The filled fields
 *
 * @return Status::OK if the HostGroup is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 */
grpc::Status engine_impl::GetHostGroup(grpc::ServerContext* context
                                       [[maybe_unused]],
                                       const NameIdentifier* request,
                                       EngineHostGroup* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>(
      [&err, request, hostgroup = response]() -> int32_t {
        std::shared_ptr<com::centreon::engine::hostgroup> selectedhostgroup;
        auto ithostgroup = hostgroup::hostgroups.find(request->name());
        if (ithostgroup != hostgroup::hostgroups.end())
          selectedhostgroup = ithostgroup->second;
        else {
          err = fmt::format("could not find hostgroup '{}'", request->name());
          return 1;
        }

        hostgroup->set_id(selectedhostgroup->get_id());
        hostgroup->set_name(selectedhostgroup->get_group_name());
        hostgroup->set_alias(selectedhostgroup->get_alias());

        if (!selectedhostgroup->members.empty())
          for (const auto& [key, _] : selectedhostgroup->members)
            hostgroup->add_members(key);

        hostgroup->set_notes(selectedhostgroup->get_notes());
        hostgroup->set_notes_url(selectedhostgroup->get_notes_url());
        hostgroup->set_action_url(selectedhostgroup->get_action_url());

        return 0;
      });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return ServiceGroup informations.
 *
 * @param context gRPC context
 * @param request ServiceGroup's identifier (by ServiceGroup name)
 * @param response The filled fields
 *
 * @return Status::OK if the ServiceGroup is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 */
grpc::Status engine_impl::GetServiceGroup(grpc::ServerContext* context
                                          [[maybe_unused]],
                                          const NameIdentifier* request,
                                          EngineServiceGroup* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>([&err, request,
                                           servicegroup =
                                               response]() -> int32_t {
    std::shared_ptr<com::centreon::engine::servicegroup> selectedservicegroup;
    auto itservicegroup = servicegroup::servicegroups.find(request->name());
    if (itservicegroup != servicegroup::servicegroups.end())
      selectedservicegroup = itservicegroup->second;
    else {
      err = fmt::format("could not find servicegroup '{}'", request->name());
      return 1;
    }
    servicegroup->set_id(selectedservicegroup->get_id());
    servicegroup->set_name(selectedservicegroup->get_group_name());
    servicegroup->set_alias(selectedservicegroup->get_alias());

    servicegroup->set_notes(selectedservicegroup->get_notes());
    servicegroup->set_notes_url(selectedservicegroup->get_notes_url());
    servicegroup->set_action_url(selectedservicegroup->get_action_url());

    if (!selectedservicegroup->members.empty()) {
      for (const auto& [host_serv_pair, _] : selectedservicegroup->members) {
        servicegroup->add_members(
            fmt::format("{},{}", host_serv_pair.first, host_serv_pair.second));
      }
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return ContactGroup informations.
 *
 * @param context gRPC context
 * @param request ContactGroup's identifier (by ContactGroup name)
 * @param response The filled fields
 *
 * @return Status::OK if the ContactGroup is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 */
grpc::Status engine_impl::GetContactGroup(grpc::ServerContext* context
                                          [[maybe_unused]],
                                          const NameIdentifier* request,
                                          EngineContactGroup* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>([&err, request,
                                           contactgroup =
                                               response]() -> int32_t {
    std::shared_ptr<com::centreon::engine::contactgroup> selectedcontactgroup;
    auto itcontactgroup = contactgroup::contactgroups.find(request->name());
    if (itcontactgroup != contactgroup::contactgroups.end())
      selectedcontactgroup = itcontactgroup->second;
    else {
      err = fmt::format("could not find contactgroup '{}'", request->name());
      return 1;
    }

    contactgroup->set_name(selectedcontactgroup->get_name());
    contactgroup->set_alias(selectedcontactgroup->get_alias());

    if (!selectedcontactgroup->get_members().empty())
      for (const auto& [key, _] : selectedcontactgroup->get_members())
        contactgroup->add_members(key);

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return Tag informations.
 *
 * @param context gRPC context
 * @param request Tag's identifier (by Tag id and type)
 * @param response The filled fields
 *
 * @return Status::OK if the tag is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 */
grpc::Status engine_impl::GetTag(grpc::ServerContext* context [[maybe_unused]],
                                 const IdOrTypeIdentifier* request,
                                 EngineTag* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>([&err, request,
                                           tag = response]() -> int32_t {
    std::shared_ptr<com::centreon::engine::tag> selectedtag;
    auto ittag = tag::tags.find(std::make_pair(request->id(), request->type()));
    if (ittag != tag::tags.end())
      selectedtag = ittag->second;
    else {
      err = fmt::format("could not find tag id:'{}', type:'{}' ", request->id(),
                        request->type());
      return 1;
    }

    tag->set_name(selectedtag->name());
    tag->set_id(selectedtag->id());
    tag->set_type(static_cast<EngineTag::TagType>(selectedtag->type()));

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return Severity informations.
 *
 * @param context gRPC context
 * @param request Severity's identifier (by Severity id and type)
 * @param response The filled fields
 *
 * @return Status::OK if the Severity is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 */
grpc::Status engine_impl::GetSeverity(grpc::ServerContext* context
                                      [[maybe_unused]],
                                      const IdOrTypeIdentifier* request,
                                      EngineSeverity* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>(
      [&err, request, severity = response]() -> int32_t {
        std::shared_ptr<com::centreon::engine::severity> selectedseverity;
        auto itseverity = severity::severities.find(
            std::make_pair(request->id(), request->type()));
        if (itseverity != severity::severities.end())
          selectedseverity = itseverity->second;
        else {
          err = fmt::format("could not find tag id:'{}', type:'{}' ",
                            request->id(), request->type());
          return 1;
        }

        severity->set_name(selectedseverity->name());
        severity->set_id(selectedseverity->id());
        severity->set_type(static_cast<EngineSeverity::SeverityType>(
            selectedseverity->type() + 1));
        severity->set_level(selectedseverity->level());
        severity->set_icon_id(selectedseverity->icon_id());

        return 0;
      });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return Command informations.
 *
 * @param context gRPC context
 * @param request Command's identifier (by name)
 * @param response The filled fields
 *
 * @return Status::OK if the Command is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 */
grpc::Status engine_impl::GetCommand(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const NameIdentifier* request,
                                     EngineCommand* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>(
      [&err, request, command = response]() -> int32_t {
        std::shared_ptr<commands::command> selectedcommand;
        auto itcommand = commands::command::commands.find(request->name());
        if (itcommand != commands::command::commands.end())
          selectedcommand = itcommand->second;
        else {
          err = fmt::format("could not find Command '{}'", request->name());
          return 1;
        }
        command->set_command_name(selectedcommand->get_name());
        command->set_command_line(selectedcommand->get_command_line());
        command->set_type(
            static_cast<EngineCommand::CmdType>(selectedcommand->get_type()));

        return 0;
      });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return Connector informations.
 *
 * @param context gRPC context
 * @param request Connector's identifier (by name)
 * @param response The filled fields
 *
 * @return Status::OK if the Connector is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 */
grpc::Status engine_impl::GetConnector(grpc::ServerContext* context
                                       [[maybe_unused]],
                                       const NameIdentifier* request,
                                       EngineConnector* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>([&err, request,
                                           connector = response]() -> int32_t {
    std::shared_ptr<com::centreon::engine::commands::connector>
        selectedconnector;
    auto itconnector = commands::connector::connectors.find(request->name());
    if (itconnector != commands::connector::connectors.end())
      selectedconnector = itconnector->second;
    else {
      err = fmt::format("could not find Connector '{}'", request->name());
      return 1;
    }
    connector->set_connector_name(selectedconnector->get_name());
    connector->set_connector_line(selectedconnector->get_command_line());

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return HostEscalation informations.
 *
 * @param context gRPC context
 * @param request HostEscalation's identifier (by name)
 * @param response The filled fields
 *
 * @return Status::OK if the HostEscalation is found and populated successfully,
 * otherwise returns Status::INVALID_ARGUMENT with an error message.
 */
grpc::Status engine_impl::GetHostEscalation(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const NameIdentifier* request,
                                            EngineHostEscalation* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>([&err, request,
                                           escalation = response]() -> int32_t {
    std::shared_ptr<com::centreon::engine::hostescalation> selectedescalation;
    auto itescalation = hostescalation::hostescalations.find(request->name());
    if (itescalation != hostescalation::hostescalations.end())
      selectedescalation = itescalation->second;
    else {
      err = fmt::format("could not find hostescalation '{}'", request->name());
      return 1;
    }
    escalation->set_host_name(selectedescalation->get_hostname());
    if (!selectedescalation->get_contactgroups().empty())
      for (const auto& [name, _] : selectedescalation->get_contactgroups())
        escalation->add_contact_group(name);

    escalation->set_first_notification(
        selectedescalation->get_first_notification());
    escalation->set_last_notification(
        selectedescalation->get_last_notification());
    escalation->set_notification_interval(
        selectedescalation->get_notification_interval());
    escalation->set_escalation_period(
        selectedescalation->get_escalation_period());
    auto options = fmt::format(
        "{}{}{}",
        selectedescalation->get_escalate_on(notifier::down) ? "d" : "",
        selectedescalation->get_escalate_on(notifier::unreachable) ? "u" : "",
        selectedescalation->get_escalate_on(notifier::up) ? "r" : "");

    if (options == "dur")
      options = "all";

    if (!options.empty() && options != "all" && options.length() != 1)
      options = fmt::format("{}", fmt::join(options, ","));

    escalation->set_escalation_option(options);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return ServiceEscalation informations.
 *
 * @param context gRPC context
 * @param request ServiceEscalation's identifier (by host and service name)
 * @param response The filled fields
 *
 * @return Status::OK if the ServiceEscalation is found and populated
 * successfully, otherwise returns Status::INVALID_ARGUMENT with an error
 * message.
 */
grpc::Status engine_impl::GetServiceEscalation(
    grpc::ServerContext* context [[maybe_unused]],
    const PairNamesIdentifier* request,
    EngineServiceEscalation* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>([&err, request,
                                           escalation = response]() -> int32_t {
    std::shared_ptr<com::centreon::engine::serviceescalation>
        selectedescalation;
    auto itescalation = serviceescalation::serviceescalations.find(
        std::make_pair(request->host_name(), request->service_name()));
    if (itescalation != serviceescalation::serviceescalations.end())
      selectedescalation = itescalation->second;
    else {
      err = fmt::format(
          "could not find serviceescalation with : host '{}',service '{}'",
          request->host_name(), request->service_name());
      return 1;
    }
    escalation->set_host(selectedescalation->get_hostname());
    escalation->set_service_description(selectedescalation->get_description());
    if (!selectedescalation->get_contactgroups().empty())
      for (const auto& [name, _] : selectedescalation->get_contactgroups())
        escalation->add_contact_group(name);

    escalation->set_first_notification(
        selectedescalation->get_first_notification());
    escalation->set_last_notification(
        selectedescalation->get_last_notification());
    escalation->set_notification_interval(
        selectedescalation->get_notification_interval());
    escalation->set_escalation_period(
        selectedescalation->get_escalation_period());

    auto options = fmt::format(
        "{}{}{}{}",
        selectedescalation->get_escalate_on(notifier::warning) ? "w" : "",
        selectedescalation->get_escalate_on(notifier::unknown) ? "u" : "",
        selectedescalation->get_escalate_on(notifier::critical) ? "c" : "",
        selectedescalation->get_escalate_on(notifier::ok) ? "r" : "");

    if (options == "wucr")
      options = "all";

    if (!options.empty() && options != "all" && options.length() != 1)
      options = fmt::format("{}", fmt::join(options, ","));

    escalation->set_escalation_option(options);

    return 0;
  });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return the total number of hosts.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 *@return Status::OK
 */
grpc::Status engine_impl::GetHostsCount(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const ::google::protobuf::Empty* request
                                        [[maybe_unused]],
                                        GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return host::hosts.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());

  return grpc::Status::OK;
}

/**
 * @brief Return the total number of contacts.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */

grpc::Status engine_impl::GetContactsCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return contact::contacts.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());

  return grpc::Status::OK;
}

/**
 * @brief Return the total number of services.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetServicesCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return service::services.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of service groups.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 *@return Status::OK
 */
grpc::Status engine_impl::GetServiceGroupsCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return servicegroup::servicegroups.size(); });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of contact groups.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetContactGroupsCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return contactgroup::contactgroups.size(); });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of host groups.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetHostGroupsCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return hostgroup::hostgroups.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of service dependencies.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetServiceDependenciesCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>([]() -> int32_t {
    return servicedependency::servicedependencies.size();
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of host dependencies.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetHostDependenciesCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return hostdependency::hostdependencies.size(); });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Add a comment into a host.
 *
 * @param context gRPC context
 * @param request EngineComment. EngineComment requires differents
 * fields to add a host comment. theses fields are :
 *    host name
 *    user name
 *    the comment
 *    persistent value
 *    entry time value
 * @param response Command Success
 *
 * @return Status::OK
 */
grpc::Status engine_impl::AddHostComment(grpc::ServerContext* context
                                         [[maybe_unused]],
                                         const EngineComment* request,
                                         CommandSuccess* response
                                         [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    /* add the comment */
    auto cmt = std::make_shared<comment>(
        comment::host, comment::user, temp_host->host_id(), 0,
        request->entry_time(), request->user(), request->comment_data(),
        request->persistent(), comment::external, false, (time_t)0);
    comment::comments.insert({cmt->get_comment_id(), cmt});
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Add a comment into a service.
 *
 * @param context gRPC context
 * @param request EngineComment. EngineComment requires differents
 * fields to add a service comment. theses fields are :
 *    host name
 *    service description
 *    user name
 *    the comment
 *    persistent value
 *    entry time value
 * @param response Command Success
 *
 * @return Status::OK
 */
grpc::Status engine_impl::AddServiceComment(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const EngineComment* request,
                                            CommandSuccess* response
                                            [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    std::shared_ptr<engine::service> temp_service;
    /* get the service */
    auto it =
        service::services.find({request->host_name(), request->svc_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr) {
      err = fmt::format("could not find service ('{}', '{}')",
                        request->host_name(), request->svc_desc());
      return 1;
    }
    auto it2 = host::hosts.find(request->host_name());
    if (it2 != host::hosts.end())
      temp_host = it2->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    /* add the comment */
    auto cmt = std::make_shared<comment>(
        comment::service, comment::user, temp_host->host_id(),
        temp_service->service_id(), request->entry_time(), request->user(),
        request->comment_data(), request->persistent(), comment::external,
        false, (time_t)0);
    if (!cmt) {
      err =
          fmt::format("could not insert comment '{}'", request->comment_data());
      return 1;
    }
    comment::comments.insert({cmt->get_comment_id(), cmt});
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Remove a host or service comment from the status log.
 *
 * @param context gRPC context
 * @param request GenericValue. Id of the comment
 * @param response Command Success
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteComment(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const GenericValue* request,
                                        CommandSuccess* response
                                        [[maybe_unused]]) {
  uint32_t comment_id = request->value();
  std::string err;
  if (comment_id == 0)
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "comment_id must not be set to 0");

  auto fn = std::packaged_task<int32_t(void)>([&err, &comment_id]() -> int32_t {
    if (comment::delete_comment(comment_id))
      return 0;
    else {
      err = fmt::format("could not delete comment with id {}", comment_id);
      return 1;
    }
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Removes all comments from a host.
 *
 * @param context gRPC context
 * @param request Host's identifier (it can be a hostname or a hostid)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteAllHostComments(
    grpc::ServerContext* context [[maybe_unused]],
    const NameOrIdIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    std::tie(temp_host, err) = get_host(*request);
    if (!err.empty()) {
      return 1;
    }
    comment::delete_host_comments(temp_host->host_id());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Removes all comments from a service.
 *
 * @param context gRPC context
 * @param request Service's identifier (it can be a hostname & servicename or a
 * hostid & serviceid)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteAllServiceComments(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    std::tie(temp_service, err) = get_serv(*request);
    if (!err.empty()) {
      return 1;
    }

    comment::delete_service_comments(temp_service->host_id(),
                                     temp_service->service_id());
    return 0;
  });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Removes a host acknowledgement.
 *
 * @param context gRPC context
 * @param request Host's identifier (it can be a hostname or hostid)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::RemoveHostAcknowledgement(
    grpc::ServerContext* context [[maybe_unused]],
    const NameOrIdIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    std::tie(temp_host, err) = get_host(*request);
    if (!err.empty()) {
      return 1;
    }

    /* set the acknowledgement flag */
    temp_host->set_acknowledgement(AckType::NONE);
    /* update the status log with the host info */
    temp_host->update_status(host::STATUS_ACKNOWLEDGEMENT);
    /* remove any non-persistant comments associated with the ack */
    comment::delete_host_acknowledgement_comments(temp_host.get());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Removes a service acknowledgement.
 *
 * @param context gRPC context
 * @param request Service's identifier (it can be a hostname & servicename or a
 * hostid & serviceid)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::RemoveServiceAcknowledgement(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    std::tie(temp_service, err) = get_serv(*request);
    if (!err.empty()) {
      return 1;
    }

    /* set the acknowledgement flag */
    temp_service->set_acknowledgement(AckType::NONE);
    /* update the status log with the service info */
    temp_service->update_status(service::STATUS_ACKNOWLEDGEMENT);
    /* remove any non-persistant comments associated with the ack */
    comment::delete_service_acknowledgement_comments(temp_service.get());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::AcknowledgementHostProblem(
    grpc::ServerContext* context [[maybe_unused]],
    const EngineAcknowledgement* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    /* cannot acknowledge a non-existent problem */
    if (temp_host->get_current_state() == host::state_up) {
      err = fmt::format("state of host '{}' is up", request->host_name());
      return 1;
    }
    /* set the acknowledgement flag */
    if (request->type() == EngineAcknowledgement_Type_STICKY)
      temp_host->set_acknowledgement(AckType::STICKY);
    else
      temp_host->set_acknowledgement(AckType::NORMAL);
    /* schedule acknowledgement expiration */
    time_t current_time(time(nullptr));
    temp_host->set_last_acknowledgement(current_time);
    temp_host->schedule_acknowledgement_expiration();
    /* send data to event broker */
    broker_acknowledgement_data(temp_host.get(), request->ack_author().c_str(),
                                request->ack_data().c_str(), request->type(),
                                request->notify(), request->persistent());
    /* send out an acknowledgement notification */
    if (request->notify())
      temp_host->notify(notifier::reason_acknowledgement, request->ack_author(),
                        request->ack_data(),
                        notifier::notification_option_none);
    /* update the status log with the host info */
    temp_host->update_status(host::STATUS_ACKNOWLEDGEMENT);
    /* add a comment for the acknowledgement */
    auto com = std::make_shared<comment>(
        comment::host, comment::acknowledgment, temp_host->host_id(), 0,
        current_time, request->ack_author(), request->ack_data(),
        request->persistent(), comment::internal, false, (time_t)0);
    comment::comments.insert({com->get_comment_id(), com});

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::AcknowledgementServiceProblem(
    grpc::ServerContext* context [[maybe_unused]],
    const EngineAcknowledgement* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    auto it =
        service::services.find({request->host_name(), request->service_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr) {
      err = fmt::format("could not find service '{}', '{}'",
                        request->host_name(), request->service_desc());
      return 1;
    }
    /* cannot acknowledge a non-existent problem */
    if (temp_service->get_current_state() == service::state_ok) {
      err = fmt::format("state of service '{}', '{}' is up",
                        request->host_name(), request->service_desc());
      return 1;
    }
    /* set the acknowledgement flag */
    if (request->type() == EngineAcknowledgement_Type_STICKY)
      temp_service->set_acknowledgement(AckType::STICKY);
    else
      temp_service->set_acknowledgement(AckType::NORMAL);
    /* schedule acknowledgement expiration */
    time_t current_time = time(nullptr);
    temp_service->set_last_acknowledgement(current_time);
    temp_service->schedule_acknowledgement_expiration();
    /* send data to event broker */
    broker_acknowledgement_data(temp_service.get(),
                                request->ack_author().c_str(),
                                request->ack_data().c_str(), request->type(),
                                request->notify(), request->persistent());
    /* send out an acknowledgement notification */
    if (request->notify())
      temp_service->notify(notifier::reason_acknowledgement,
                           request->ack_author(), request->ack_data(),
                           notifier::notification_option_none);
    /* update the status log with the service info */
    temp_service->update_status(service::STATUS_ACKNOWLEDGEMENT);

    /* add a comment for the acknowledgement */
    auto com = std::make_shared<comment>(
        comment::service, comment::acknowledgment, temp_service->host_id(),
        temp_service->service_id(), current_time, request->ack_author(),
        request->ack_data(), request->persistent(), comment::internal, false,
        (time_t)0);
    comment::comments.insert({com->get_comment_id(), com});
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Schedules downtime for a specific host.
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields for a host are :
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response) {
  if (request->host_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    uint64_t downtime_id = 0;
    unsigned long duration;
    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());
    /* scheduling downtime */
    int res = downtime_manager::instance().schedule_downtime(
        downtime::host_downtime, temp_host->host_id(), 0, request->entry_time(),
        request->author().c_str(), request->comment_data().c_str(),
        request->start(), request->end(), request->fixed(),
        request->triggered_by(), duration, &downtime_id);
    if (res == ERROR) {
      err = fmt::format("could not schedule downtime of host '{}'",
                        request->host_name());
      return 1;
    } else {
      return 0;
    }
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for a specific service.
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields for a service are :
 *  host name
 *  service description
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleServiceDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty() || request->service_desc().empty() ||
      request->author().empty() || request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    uint64_t downtime_id(0);
    unsigned long duration;
    /* get the service */
    auto it =
        service::services.find({request->host_name(), request->service_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr) {
      err = fmt::format("could not find service '{}', '{}'",
                        request->host_name(), request->service_desc());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* scheduling downtime */
    int res = downtime_manager::instance().schedule_downtime(
        downtime::service_downtime, temp_service->host_id(),
        temp_service->service_id(), request->entry_time(),
        request->author().c_str(), request->comment_data().c_str(),
        request->start(), request->end(), request->fixed(),
        request->triggered_by(), duration, &downtime_id);
    if (res == ERROR) {
      err = fmt::format("could not schedule downtime of service '{}', '{}'",
                        request->host_name(), request->service_desc());
      return 1;
    } else
      return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for all services from a specific host.
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostServicesDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    uint64_t downtime_id(0);
    unsigned long duration;
    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    for (service_map_unsafe::iterator it(temp_host->services.begin()),
         end(temp_host->services.end());
         it != end; ++it) {
      if (!it->second)
        continue;
      /* scheduling downtime */
      downtime_manager::instance().schedule_downtime(
          downtime::service_downtime, temp_host->host_id(),
          it->second->service_id(), request->entry_time(),
          request->author().c_str(), request->comment_data().c_str(),
          request->start(), request->end(), request->fixed(),
          request->triggered_by(), duration, &downtime_id);
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for all hosts from a specific host group name.
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  host group name
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostGroupHostsDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_group_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    uint64_t downtime_id = 0;
    unsigned long duration;
    hostgroup* hg{nullptr};
    /* get the host group */
    hostgroup_map::const_iterator it(
        hostgroup::hostgroups.find(request->host_group_name()));
    if (it == hostgroup::hostgroups.end() || !it->second) {
      err = fmt::format("could not find host group name '{}'",
                        request->host_group_name());
      return 1;
    }
    hg = it->second.get();
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* iterate through host group members(hosts) */
    for (host_map_unsafe::iterator it(hg->members.begin()),
         end(hg->members.end());
         it != end; ++it)
      /* scheduling downtime */
      downtime_manager::instance().schedule_downtime(
          downtime::host_downtime, it->second->host_id(), 0,
          request->entry_time(), request->author().c_str(),
          request->comment_data().c_str(), request->start(), request->end(),
          request->fixed(), request->triggered_by(), duration, &downtime_id);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for all services belonging
 * to the hosts of the host group name.
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  host group name
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostGroupServicesDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_group_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    uint64_t downtime_id(0);
    unsigned long duration;
    hostgroup* hg{nullptr};
    /* get the hostgroup */
    hostgroup_map::const_iterator it(
        hostgroup::hostgroups.find(request->host_group_name()));
    if (it == hostgroup::hostgroups.end() || !it->second) {
      err = fmt::format("could not find host group name '{}'",
                        request->host_group_name());
      return 1;
    }
    hg = it->second.get();
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* iterate through host group members(hosts) */
    for (host_map_unsafe::iterator it(hg->members.begin()),
         end(hg->members.end());
         it != end; ++it) {
      if (!it->second)
        continue;
      /* iterate through services of the current host */
      for (service_map_unsafe::iterator it2(it->second->services.begin()),
           end2(it->second->services.end());
           it2 != end2; ++it2) {
        if (!it2->second)
          continue;
        /* scheduling downtime */
        downtime_manager::instance().schedule_downtime(
            downtime::service_downtime, it2->second->host_id(),
            it2->second->service_id(), request->entry_time(),
            request->author().c_str(), request->comment_data().c_str(),
            request->start(), request->end(), request->fixed(),
            request->triggered_by(), duration, &downtime_id);
      }
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for all host from a service group name
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  service group name
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleServiceGroupHostsDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->service_group_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    host* temp_host{nullptr};
    host* last_host{nullptr};
    uint64_t downtime_id(0);
    unsigned long duration;
    servicegroup_map::const_iterator sg_it;
    /* verify that the servicegroup is valid */
    sg_it = servicegroup::servicegroups.find(request->service_group_name());
    if (sg_it == servicegroup::servicegroups.end() || !sg_it->second) {
      err = fmt::format("could not find servicegroupname '{}'",
                        request->service_group_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    for (service_map_unsafe::iterator it(sg_it->second->members.begin()),
         end(sg_it->second->members.end());
         it != end; ++it) {
      /* get the host to schedule */
      host_map::const_iterator found(host::hosts.find(it->first.first));
      if (found == host::hosts.end() || !found->second)
        continue;
      temp_host = found->second.get();
      if (last_host == temp_host)
        continue;
      /* scheduling downtime */
      downtime_manager::instance().schedule_downtime(
          downtime::host_downtime, it->second->host_id(), 0,
          request->entry_time(), request->author().c_str(),
          request->comment_data().c_str(), request->start(), request->end(),
          request->fixed(), request->triggered_by(), duration, &downtime_id);
      last_host = temp_host;
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for all services from a service group name
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  service group name
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleServiceGroupServicesDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->service_group_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    uint64_t downtime_id(0);
    unsigned long duration;
    servicegroup_map::const_iterator sg_it;
    /* verify that the servicegroup is valid */
    sg_it = servicegroup::servicegroups.find(request->service_group_name());
    if (sg_it == servicegroup::servicegroups.end() || !sg_it->second) {
      err = fmt::format("could not find servicegroupname '{}'",
                        request->service_group_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* iterate through the services of service group */
    for (service_map_unsafe::iterator it(sg_it->second->members.begin()),
         end(sg_it->second->members.end());
         it != end; ++it)
      /* scheduling downtime */
      downtime_manager::instance().schedule_downtime(
          downtime::service_downtime, it->second->host_id(),
          it->second->service_id(), request->entry_time(),
          request->author().c_str(), request->comment_data().c_str(),
          request->start(), request->end(), request->fixed(),
          request->triggered_by(), duration, &downtime_id);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for a specific host and his childrens
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleAndPropagateHostDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    uint64_t downtime_id(0);
    unsigned long duration;
    /* get the main host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* scheduling the parent host */
    downtime_manager::instance().schedule_downtime(
        downtime::host_downtime, temp_host->host_id(), 0, request->entry_time(),
        request->author().c_str(), request->comment_data().c_str(),
        request->start(), request->end(), request->fixed(),
        request->triggered_by(), duration, &downtime_id);

    /* schedule (non-triggered) downtime for all child hosts */
    command_manager::schedule_and_propagate_downtime(
        temp_host.get(), request->entry_time(), request->author().c_str(),
        request->comment_data().c_str(), request->start(), request->end(),
        request->fixed(), 0, duration);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for a specific host and his childrens with a
 * triggered id
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */

grpc::Status engine_impl::ScheduleAndPropagateTriggeredHostDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    uint64_t downtime_id(0);
    unsigned long duration;
    /* get the main host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* scheduling the parent host */
    downtime_manager::instance().schedule_downtime(
        downtime::host_downtime, temp_host->host_id(), 0, request->entry_time(),
        request->author().c_str(), request->comment_data().c_str(),
        request->start(), request->end(), request->fixed(),
        request->triggered_by(), duration, &downtime_id);
    /* scheduling his childs */
    command_manager::schedule_and_propagate_downtime(
        temp_host.get(), request->entry_time(), request->author().c_str(),
        request->comment_data().c_str(), request->start(), request->end(),
        request->fixed(), downtime_id, duration);

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief  Deletes scheduled downtime
 *
 * @param context gRPC context
 * @param request GenericValue. GenericValue is a downtime id
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteDowntime(grpc::ServerContext* context
                                         [[maybe_unused]],
                                         const GenericValue* request,
                                         CommandSuccess* response
                                         [[maybe_unused]]) {
  uint32_t downtime_id = request->value();
  std::string err;
  auto fn =
      std::packaged_task<int32_t(void)>([&err, &downtime_id]() -> int32_t {
        /* deletes scheduled  downtime */
        if (downtime_manager::instance().unschedule_downtime(downtime_id) ==
            ERROR) {
          err = fmt::format("could not delete downtime {}", downtime_id);
          return 1;
        } else
          return 0;
      });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Delete scheduled host  downtime, according to some criterias.
 * If a criteria is not defined then it doesnt matter in the search
 * for downtime
 *
 * @param context gRPC context
 * @param request DowntimeCriterias (it can be a hostname, a start_time,
 * an end_time, a fixed, a triggered_by, a duration, an author
 * or a comment)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteHostDowntimeFull(
    grpc::ServerContext* context [[maybe_unused]],
    const DowntimeCriterias* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::list<std::shared_ptr<downtimes::downtime>> dtlist;
    for (auto it = downtimes::downtime_manager::instance()
                       .get_scheduled_downtimes()
                       .begin(),
              end = downtimes::downtime_manager::instance()
                        .get_scheduled_downtimes()
                        .end();
         it != end; ++it) {
      auto dt = it->second;
      uint64_t host_id = engine::get_host_id(request->host_name());
      if (!request->host_name().empty() && host_id != dt->host_id())
        continue;
      if (request->has_start() &&
          dt->get_start_time() != request->start().value())
        continue;
      if (request->has_end() && dt->get_end_time() != request->end().value())
        continue;
      if (request->has_fixed() && dt->is_fixed() != request->fixed().value())
        continue;
      if (request->has_triggered_by() &&
          dt->get_triggered_by() != request->triggered_by().value())
        continue;
      if (request->has_duration() &&
          dt->get_duration() != request->duration().value())
        continue;
      if (!(request->author().empty()) && dt->get_author() != request->author())
        continue;
      if (!(request->comment_data().empty()) &&
          dt->get_comment() != request->comment_data())
        continue;
      dtlist.push_back(dt);
    }

    for (auto& d : dtlist)
      downtime_manager::instance().unschedule_downtime(d->get_downtime_id());

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Delete scheduled service downtime, according to some criterias.
 * If a criteria is not defined then it doesnt matter in the search
 * for downtime
 *
 * @param context gRPC context
 * @param request DowntimeCriterias (it can be a hostname, a service description
 * a start_time, an end_time, a fixed, a triggered_by, a duration, an author
 * or a comment)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteServiceDowntimeFull(
    grpc::ServerContext* context [[maybe_unused]],
    const DowntimeCriterias* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::list<service_downtime*> dtlist;
    /* iterate through all current downtime(s) */
    for (auto it = downtimes::downtime_manager::instance()
                       .get_scheduled_downtimes()
                       .begin(),
              end = downtimes::downtime_manager::instance()
                        .get_scheduled_downtimes()
                        .end();
         it != end; ++it) {
      service_downtime* dt = static_cast<service_downtime*>(it->second.get());
      /* we are checking if request criteria match with the downtime criteria
       */
      auto p =
          engine::get_host_and_service_names(dt->host_id(), dt->service_id());
      if (!request->host_name().empty() && p.first != request->host_name())
        continue;
      if (!request->service_desc().empty() &&
          p.second != request->service_desc())
        continue;
      if (request->has_start() &&
          dt->get_start_time() != request->start().value())
        continue;
      if (request->has_end() && dt->get_end_time() != request->end().value())
        continue;
      if (request->has_fixed() && dt->is_fixed() != request->fixed().value())
        continue;
      if (request->has_triggered_by() &&
          dt->get_triggered_by() != request->triggered_by().value())
        continue;
      if (request->has_duration() &&
          dt->get_duration() != request->duration().value())
        continue;
      if (!(request->author().empty()) && dt->get_author() != request->author())
        continue;
      if (!(request->comment_data().empty()) &&
          dt->get_comment() != request->comment_data())
        continue;
      /* if all criterias match then we found a downtime to delete */
      dtlist.push_back(dt);
    }

    /* deleting downtime(s) */
    for (auto& d : dtlist)
      downtime_manager::instance().unschedule_downtime(d->get_downtime_id());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Deletes scheduled host and service downtime based on hostname and
 * optionnaly other filter arguments.
 *
 * @param context gRPC context
 * @param request DowntimeHostIdentifier (it's a hostname and optionally other
 * filter arguments like service description, start time and downtime's
 * comment
 * @param Command response
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteDowntimeByHostName(
    grpc::ServerContext* context [[maybe_unused]],
    const DowntimeHostIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  /*hostname must be defined to delete the downtime but not others arguments*/
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, &host_name,
                                               request]() -> int32_t {
    std::pair<bool, time_t> start_time;
    std::string service_desc;
    std::string comment_data;
    if (!(request->service_desc().empty()))
      service_desc = request->service_desc();
    if (!(request->comment_data().empty()))
      comment_data = request->comment_data();
    if (!(request->has_start()))
      start_time = {false, 0};
    else
      start_time = {true, request->start().value()};

    uint32_t deleted =
        downtime_manager::instance()
            .delete_downtime_by_hostname_service_description_start_time_comment(
                host_name, service_desc, start_time, comment_data);
    if (deleted == 0) {
      err = fmt::format("could not delete downtime with hostname '{}'",
                        request->host_name());
      return 1;
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Deletes scheduled host and service downtime based on hostgroupname
 * and optionnaly other filter arguments.
 *
 * @param context gRPC context
 * @param request DowntimeHostIdentifier (it's a hostname and optionally other
 * filter arguments like service description, start time and downtime's
 * comment
 * @param Command response
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteDowntimeByHostGroupName(
    grpc::ServerContext* context [[maybe_unused]],
    const DowntimeHostGroupIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string const& host_group_name = request->host_group_name();
  if (host_group_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_group_name must not be empty");
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, &host_group_name,
                                               request]() -> int32_t {
    std::pair<bool, time_t> start_time;
    std::string host_name;
    std::string service_desc;
    std::string comment_data;
    uint32_t deleted = 0;

    auto it = hostgroup::hostgroups.find(host_group_name);
    if (it == hostgroup::hostgroups.end() || !it->second) {
      err = fmt::format("could not find host group name '{}'",
                        request->host_group_name());
      return 1;
    }
    if (!(request->host_name().empty()))
      host_name = request->host_name();
    if (!(request->service_desc().empty()))
      service_desc = request->service_desc();
    if (!(request->comment_data().empty()))
      comment_data = request->comment_data();
    if (!(request->has_start()))
      start_time = {false, 0};
    else
      start_time = {true, request->start().value()};

    for (host_map_unsafe::iterator it_h(it->second->members.begin()),
         end_h(it->second->members.end());
         it_h != end_h; ++it_h) {
      if (!it_h->second)
        continue;
      if (!(host_name.empty()) && it_h->first != host_name)
        continue;
      deleted =
          downtime_manager::instance()
              .delete_downtime_by_hostname_service_description_start_time_comment(
                  host_name, service_desc, start_time, comment_data);
    }

    if (deleted == 0) {
      err = fmt::format("could not delete downtime with host group name '{}'",
                        request->host_group_name());
      return 1;
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Deletes scheduled host and service downtime based on start time,
 * the comment and optionnaly other filter arguments.
 *
 * @param context gRPC context
 * @param request DowntimeHostIdentifier (it's a hostname and optionally other
 * filter arguments like service description, start time and downtime's
 * comment
 * @param Command response
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteDowntimeByStartTimeComment(
    grpc::ServerContext* context [[maybe_unused]],
    const DowntimeStartTimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  time_t start_time;
  /*hostname must be defined to delete the downtime but not others arguments*/
  if (!(request->has_start()))
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "start_time must not be empty");
  else
    start_time = request->start().value();

  std::string const& comment_data = request->comment_data();
  if (comment_data.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "comment_data must not be empty");
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, &comment_data,
                                               &start_time]() -> int32_t {
    uint32_t deleted =
        downtime_manager::instance()
            .delete_downtime_by_hostname_service_description_start_time_comment(
                "", "", {true, start_time}, comment_data);
    if (0 == deleted) {
      err = fmt::format("could not delete comment with comment_data '{}'",
                        comment_data);
      return 1;
    }
    return 0;
  });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Schedules a host check at particular time
 *
 * @param context gRPC context
 * @param request HostCheckIdentifier. HostCheckIdentifier is a host name
 * and a delay time.
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostCheck(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const HostCheckIdentifier* request,
                                            CommandSuccess* response
                                            [[maybe_unused]]) {
  if (request->host_name().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    if (!request->force())
      temp_host->schedule_check(request->delay_time(), CHECK_OPTION_NONE);
    else
      temp_host->schedule_check(request->delay_time(),
                                CHECK_OPTION_FORCE_EXECUTION);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Schedules all services check from a host at particular time
 *
 * @param context gRPC context
 * @param request HostCheckIdentifier. HostCheckIdentifier is a host name
 * and a delay time.
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostServiceCheck(
    grpc::ServerContext* context [[maybe_unused]],
    const HostCheckIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;

    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    /* iterate through services of the current host */
    for (service_map_unsafe::iterator it(temp_host->services.begin()),
         end(temp_host->services.end());
         it != end; ++it) {
      if (!it->second)
        continue;
      if (!request->force())
        it->second->schedule_check(request->delay_time(), CHECK_OPTION_NONE);
      else
        it->second->schedule_check(request->delay_time(),
                                   CHECK_OPTION_FORCE_EXECUTION);
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Schedules a service check at particular time
 *
 * @param context gRPC context
 * @param request ServiceCheckIdentifier. HostCheckIdentifier is a host name,
 *  a service description and a delay time.
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleServiceCheck(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceCheckIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  if (request->service_desc().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "service description must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    /* get the service */
    auto it =
        service::services.find({request->host_name(), request->service_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr) {
      err = fmt::format("could not find service '{}', '{}'",
                        request->host_name(), request->service_desc());
      return 1;
    }
    if (!request->force())
      temp_service->schedule_check(request->delay_time(), CHECK_OPTION_NONE);
    else
      temp_service->schedule_check(request->delay_time(),
                                   CHECK_OPTION_FORCE_EXECUTION);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief  Schedules a program shutdown or restart
 *
 * @param context gRPC context
 * @param request EngineSignalProcess. HostCheckIdentifier is a process name
 * and a scheduled time.
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::SignalProcess(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const EngineSignalProcess* request,
                                        CommandSuccess* response
                                        [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::unique_ptr<timed_event> evt;
    if (EngineSignalProcess::Process_Name(request->process()) == "SHUTDOWN") {
      /* add a scheduled program shutdown or restart to the event list */
      evt = std::make_unique<timed_event>(timed_event::EVENT_PROGRAM_SHUTDOWN,
                                          request->scheduled_time(), false, 0,
                                          nullptr, false, nullptr, nullptr, 0);
    } else if (EngineSignalProcess::Process_Name(request->process()) ==
               "RESTART") {
      evt = std::make_unique<timed_event>(timed_event::EVENT_PROGRAM_RESTART,
                                          request->scheduled_time(), false, 0,
                                          nullptr, false, nullptr, nullptr, 0);
    } else {
      err = "no signal informed, you should inform a restart or a shutdown";
      return 1;
    }

    events::loop::instance().schedule(std::move(evt), true);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Delays a host notification for given number of minutes
 *
 * @param context gRPC context
 * @param request HostDelayIdentifier. HostDelayIdentifier is a host name or a
 * host id and a delay time
 * @param Command response
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DelayHostNotification(
    grpc::ServerContext* context [[maybe_unused]],
    const HostDelayIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;

    switch (request->identifier_case()) {
      case HostDelayIdentifier::kName: {
        auto it = host::hosts.find(request->name());
        if (it != host::hosts.end())
          temp_host = it->second;
        if (temp_host == nullptr) {
          err = fmt::format("could not find host '{}'", request->name());
          return 1;
        }
      } break;
      case HostDelayIdentifier::kId: {
        auto it = host::hosts_by_id.find(request->id());
        if (it != host::hosts_by_id.end())
          temp_host = it->second;
        if (temp_host == nullptr) {
          err = fmt::format("could not find host {}", request->id());
          return 1;
        }
      } break;
      default: {
        err = "could not find identifier, you should inform a real host";
        return 1;
        break;
      }
    }

    temp_host->set_next_notification(request->delay_time());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Delays a service notification for given number of minutes
 *
 * @param context gRPC context
 * @param request ServiceDelayIdentifier. ServiceDelayIdentifier is
 * a {host name and service name} or a {host id, service id} and delay time
 * @param Command response
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DelayServiceNotification(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceDelayIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;

    switch (request->identifier_case()) {
      case ServiceDelayIdentifier::kNames: {
        PairNamesIdentifier names = request->names();
        auto it =
            service::services.find({names.host_name(), names.service_name()});
        if (it != service::services.end())
          temp_service = it->second;
        if (temp_service == nullptr) {
          err = fmt::format("could not find service ('{}', '{}')",
                            names.host_name(), names.service_name());
          return 1;
        }
      } break;
      case ServiceDelayIdentifier::kIds: {
        PairIdsIdentifier ids = request->ids();
        auto it =
            service::services_by_id.find({ids.host_id(), ids.service_id()});
        if (it != service::services_by_id.end())
          temp_service = it->second;
        if (temp_service == nullptr) {
          err = fmt::format("could not find service ({}, {})", ids.host_id(),
                            ids.service_id());
          return 1;
        }
      } break;
      default: {
        err = "could not find identifier, you should inform a real service";
        return 1;
        break;
      }
    }

    temp_service->set_next_notification(request->delay_time());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeHostObjectIntVar(grpc::ServerContext* context
                                                 [[maybe_unused]],
                                                 const ChangeObjectInt* request,
                                                 CommandSuccess* response
                                                 [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    unsigned long attr = MODATTR_NONE;

    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    switch (request->mode()) {
      case ChangeObjectInt_Mode_NORMAL_CHECK_INTERVAL: {
        /* save the old check interval */
        double old_dval = temp_host->check_interval();

        /* modify the check interval */
        temp_host->set_check_interval(request->dval());
        attr = MODATTR_NORMAL_CHECK_INTERVAL;
        temp_host->add_modified_attributes(attr);

        /* schedule a host check if previous interval was 0 (checks were not
         * regularly scheduled) */
        if (old_dval == 0 && temp_host->active_checks_enabled()) {
          time_t preferred_time(0);
          time_t next_valid_time(0);
          /* set the host check flag */
          temp_host->set_should_be_scheduled(true);

          /* schedule a check for right now (or as soon as possible) */
          time(&preferred_time);
          if (!check_time_against_period(preferred_time,
                                         temp_host->check_period_ptr)) {
            get_next_valid_time(preferred_time, &next_valid_time,
                                temp_host->check_period_ptr);
            temp_host->set_next_check(next_valid_time);
          } else
            temp_host->set_next_check(preferred_time);

          /* schedule a check if we should */
          if (temp_host->get_should_be_scheduled())
            temp_host->schedule_check(temp_host->get_next_check(),
                                      CHECK_OPTION_NONE);
        }
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  temp_host.get(), attr);

        /* We need check result to handle next check */
        temp_host->update_status();
      } break;

      case ChangeObjectInt_Mode_RETRY_CHECK_INTERVAL:
        temp_host->set_retry_interval(request->dval());
        attr = MODATTR_RETRY_CHECK_INTERVAL;
        temp_host->set_modified_attributes(
            temp_host->get_modified_attributes() | attr);
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  temp_host.get(), attr);
        break;

      case ChangeObjectInt_Mode_MAX_ATTEMPTS:
        temp_host->set_max_attempts(request->intval());
        attr = MODATTR_MAX_CHECK_ATTEMPTS;
        temp_host->set_modified_attributes(
            temp_host->get_modified_attributes() | attr);

        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  temp_host.get(), attr);

        /* adjust current attempt number if in a hard state */
        if (temp_host->get_state_type() == notifier::hard &&
            temp_host->get_current_state() != host::state_up &&
            temp_host->get_current_attempt() > 1) {
          temp_host->set_current_attempt(temp_host->max_check_attempts());
          /* We need check result to handle next check */
          temp_host->update_status();
        }
        break;

      case ChangeObjectInt_Mode_MODATTR:
        attr = request->intval();
        temp_host->set_modified_attributes(attr);
        /* send data to event broker */
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  temp_host.get(), attr);
        break;

      default:
        err = "no mode informed for method ChangeHostObjectIntVar";
        return 1;
    }

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeServiceObjectIntVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectInt* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    unsigned long attr = MODATTR_NONE;

    auto it =
        service::services.find({request->host_name(), request->service_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr) {
      err = fmt::format("could not find service '{}', '{}'",
                        request->host_name(), request->service_desc());
      return 1;
    }
    switch (request->mode()) {
      case ChangeObjectInt_Mode_NORMAL_CHECK_INTERVAL: {
        /* save the old check interval */
        double old_dval = temp_service->check_interval();

        /* modify the check interval */
        temp_service->set_check_interval(request->dval());
        attr = MODATTR_NORMAL_CHECK_INTERVAL;

        /* schedule a service check if previous interval was 0 (checks were
         * not regularly scheduled) */
        if (old_dval == 0 && temp_service->active_checks_enabled() &&
            temp_service->check_interval() != 0) {
          time_t preferred_time(0);
          time_t next_valid_time(0);
          /* set the service check flag */
          temp_service->set_should_be_scheduled(true);

          /* schedule a check for right now (or as soon as possible) */
          time(&preferred_time);
          if (!check_time_against_period(preferred_time,
                                         temp_service->check_period_ptr)) {
            get_next_valid_time(preferred_time, &next_valid_time,
                                temp_service->check_period_ptr);
            temp_service->set_next_check(next_valid_time);
          } else
            temp_service->set_next_check(preferred_time);

          /* schedule a check if we should */
          if (temp_service->get_should_be_scheduled())
            temp_service->schedule_check(temp_service->get_next_check(),
                                         CHECK_OPTION_NONE);
        }
        temp_service->set_modified_attributes(
            temp_service->get_modified_attributes() | attr);
        broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,
                                     NEBFLAG_NONE, temp_service.get(), attr);

        /* We need check result to handle next check */
        temp_service->update_status();
      } break;
      case ChangeObjectInt_Mode_RETRY_CHECK_INTERVAL:
        temp_service->set_retry_interval(request->dval());
        attr = MODATTR_RETRY_CHECK_INTERVAL;
        temp_service->set_modified_attributes(
            temp_service->get_modified_attributes() | attr);
        /* send data to event broker */
        broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,
                                     NEBFLAG_NONE, temp_service.get(), attr);
        break;

      case ChangeObjectInt_Mode_MAX_ATTEMPTS:
        temp_service->set_max_attempts(request->intval());
        attr = MODATTR_MAX_CHECK_ATTEMPTS;
        temp_service->set_modified_attributes(
            temp_service->get_modified_attributes() | attr);

        broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,
                                     NEBFLAG_NONE, temp_service.get(), attr);

        /* adjust current attempt number if in a hard state */
        if (temp_service->get_state_type() == notifier::hard &&
            temp_service->get_current_state() != service::state_ok &&
            temp_service->get_current_attempt() > 1) {
          temp_service->set_current_attempt(temp_service->max_check_attempts());
          /* We need check result to handle next check */
          temp_service->update_status();
        }
        break;

      case ChangeObjectInt_Mode_MODATTR:
        attr = request->intval();
        temp_service->set_modified_attributes(attr);
        /* send data to event broker */
        broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,
                                     NEBFLAG_NONE, temp_service.get(), attr);
        break;
      default:
        err = "no mode informed for method ChangeServiceObjectIntVar";
        return 1;
    }

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeContactObjectIntVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeContactObjectInt* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<com::centreon::engine::contact> temp_contact;
    unsigned long attr = MODATTR_NONE;
    unsigned long hattr = MODATTR_NONE;
    unsigned long sattr = MODATTR_NONE;

    auto itcontactname = contact::contacts.find(request->contact_name());
    if (itcontactname != contact::contacts.end())
      temp_contact = itcontactname->second;
    else {
      err = fmt::format("could not find contact '{}'", request->contact_name());
      return 1;
    }

    switch (request->mode()) {
      case ChangeContactObjectInt_Mode_MODATTR:
        attr = request->intval();
        temp_contact->set_modified_attributes(attr);
        break;
      case ChangeContactObjectInt_Mode_MODHATTR:
        hattr = request->intval();
        temp_contact->set_modified_host_attributes(hattr);
        break;
      case ChangeContactObjectInt_Mode_MODSATTR:
        sattr = request->intval();
        temp_contact->set_modified_service_attributes(sattr);
        break;
      default:
        err = "no mode informed for method ChangeContactObjectIntVar";
        return 1;
    }

    return 0;
  });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    response->set_value(result.get());

  return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeHostObjectCharVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectChar* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    timeperiod* temp_timeperiod{nullptr};
    command_map::iterator cmd_found;
    unsigned long attr{MODATTR_NONE};

    /* For these cases, we verify that the host is valid */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_EVENT_HANDLER ||
        request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_COMMAND ||
        request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD ||
        request->mode() ==
            ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD) {
      auto it = host::hosts.find(request->host_name());
      if (it != host::hosts.end())
        temp_host = it->second;
      if (temp_host == nullptr) {
        err = fmt::format("could not find host '{}'", request->host_name());
        return 1;
      }
    }
    /* make sure the timeperiod is valid */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD ||
        request->mode() ==
            ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD) {
      auto found = timeperiod::timeperiods.find(request->charval());
      if (found != timeperiod::timeperiods.end())
        temp_timeperiod = found->second.get();
      if (temp_timeperiod == nullptr) {
        err = fmt::format("could not find timeperiod with value '{}'",
                          request->charval());
        return 1;
      }
    }
    /* make sure the command exists */
    else {
      cmd_found = commands::command::commands.find(request->charval());
      if (cmd_found == commands::command::commands.end() ||
          !cmd_found->second) {
        err =
            fmt::format("no command found with value '{}'", request->charval());
        return 1;
      }
    }

    /* update the variable */
    switch (request->mode()) {
      case ChangeObjectChar_Mode_CHANGE_GLOBAL_EVENT_HANDLER:
        pb_indexed_config.mut_state().set_global_host_event_handler(
            request->charval());
        global_host_event_handler_ptr = cmd_found->second.get();
        attr = MODATTR_EVENT_HANDLER_COMMAND;
        /* set the modified host attribute */
        modified_host_process_attributes |= attr;

        /* update program status */
        update_program_status(false);
        break;
      case ChangeObjectChar_Mode_CHANGE_EVENT_HANDLER:
        temp_host->set_event_handler(request->charval());
        temp_host->set_event_handler_ptr(cmd_found->second.get());
        attr = MODATTR_EVENT_HANDLER_COMMAND;
        /* set the modified host attribute */
        temp_host->add_modified_attributes(attr);
        /* send data to event broker */
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  temp_host.get(), attr);
        break;
      case ChangeObjectChar_Mode_CHANGE_CHECK_COMMAND:
        temp_host->set_check_command(request->charval());
        temp_host->set_check_command_ptr(cmd_found->second);
        attr = MODATTR_CHECK_COMMAND;
        /* send data to event broker */
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  temp_host.get(), attr);
        break;
      case ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD:
        temp_host->set_check_period(request->charval());
        temp_host->check_period_ptr = temp_timeperiod;
        attr = MODATTR_CHECK_TIMEPERIOD;
        /* send data to event broker */
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  temp_host.get(), attr);
        break;
      case ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD:
        temp_host->set_notification_period(request->charval());
        temp_host->set_notification_period_ptr(temp_timeperiod);
        attr = MODATTR_NOTIFICATION_TIMEPERIOD;
        /* send data to event broker */
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  temp_host.get(), attr);
        break;
      default:
        err = "no mode informed for method ChangeHostObjectCharVar";
        return 1;
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeServiceObjectCharVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectChar* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    timeperiod* temp_timeperiod{nullptr};
    command_map::iterator cmd_found;
    unsigned long attr{MODATTR_NONE};

    /* For these cases, we verify that the host is valid */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_EVENT_HANDLER ||
        request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_COMMAND ||
        request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD ||
        request->mode() ==
            ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD) {
      /* verify that the service is valid */
      auto it = service::services.find(
          {request->host_name(), request->service_desc()});
      if (it != service::services.end())
        temp_service = it->second;
      if (temp_service == nullptr) {
        err = fmt::format("could not find service ('{}', '{}')",
                          request->host_name(), request->service_desc());
        return 1;
      }
    }
    /* make sure the timeperiod is valid */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD ||
        request->mode() ==
            ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD) {
      auto found = timeperiod::timeperiods.find(request->charval());
      if (found != timeperiod::timeperiods.end())
        temp_timeperiod = found->second.get();
      if (temp_timeperiod == nullptr) {
        err = fmt::format("could not find timeperiod with value '{}'",
                          request->charval());
        return 1;
      }
    }
    /* make sure the command exists */
    else {
      cmd_found = commands::command::commands.find(request->charval());
      if (cmd_found == commands::command::commands.end() ||
          !cmd_found->second) {
        err =
            fmt::format("no command found with value '{}'", request->charval());
        return 1;
      }
    }

    /* update the variable */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_GLOBAL_EVENT_HANDLER) {
      pb_indexed_config.mut_state().set_global_service_event_handler(
          request->charval());
      global_service_event_handler_ptr = cmd_found->second.get();
      attr = MODATTR_EVENT_HANDLER_COMMAND;
    } else if (request->mode() == ChangeObjectChar_Mode_CHANGE_EVENT_HANDLER) {
      temp_service->set_event_handler(request->charval());
      temp_service->set_event_handler_ptr(cmd_found->second.get());
      attr = MODATTR_EVENT_HANDLER_COMMAND;
    } else if (request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_COMMAND) {
      temp_service->set_check_command(request->charval());
      temp_service->set_check_command_ptr(cmd_found->second);
      attr = MODATTR_CHECK_COMMAND;
    } else if (request->mode() ==
               ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD) {
      temp_service->set_check_period(request->charval());
      temp_service->check_period_ptr = temp_timeperiod;
      attr = MODATTR_CHECK_TIMEPERIOD;
    } else if (request->mode() ==
               ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD) {
      temp_service->set_notification_period(request->charval());
      temp_service->set_notification_period_ptr(temp_timeperiod);
      attr = MODATTR_NOTIFICATION_TIMEPERIOD;
    } else {
      err = "no mode informed for method ChangeServiceObjectCharVar";
      return 1;
    }

    /* send data to event broker and update status file */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_GLOBAL_EVENT_HANDLER) {
      /* set the modified service attribute */
      modified_service_process_attributes |= attr;

      /* update program status */
      update_program_status(false);
    } else {
      /* set the modified service attribute */
      temp_service->add_modified_attributes(attr);

      /* send data to event broker */
      broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                                   temp_service.get(), attr);
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeContactObjectCharVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeContactObjectChar* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->contact().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "contact must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request](void) -> int32_t {
    std::shared_ptr<engine::contact> temp_contact;
    timeperiod* temp_timeperiod{nullptr};
    unsigned long hattr{MODATTR_NONE};
    unsigned long sattr{MODATTR_NONE};

    auto it = contact::contacts.find(request->contact());
    if (it != contact::contacts.end())
      temp_contact = it->second;
    if (temp_contact == nullptr) {
      err = fmt::format("could not find contact '{}'", request->contact());
      return 1;
    }

    auto found = timeperiod::timeperiods.find(request->charval());
    if (found != timeperiod::timeperiods.end())
      temp_timeperiod = found->second.get();
    if (temp_timeperiod == nullptr) {
      err = fmt::format("could not find timeperiod with value '{}'",
                        request->charval());
      return 1;
    }
    if (ChangeContactObjectChar::Mode_Name(request->mode()) ==
        "CHANGE_HOST_NOTIFICATION_TIMEPERIOD") {
      temp_contact->set_host_notification_period(request->charval());
      temp_contact->set_host_notification_period_ptr(temp_timeperiod);
      hattr = MODATTR_NOTIFICATION_TIMEPERIOD;
    } else if (ChangeContactObjectChar::Mode_Name(request->mode()) ==
               "CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD") {
      temp_contact->set_service_notification_period(request->charval());
      temp_contact->set_service_notification_period_ptr(temp_timeperiod);
      hattr = MODATTR_NOTIFICATION_TIMEPERIOD;
    } else {
      err = "no mode informed for method ChangeContactObjectCharVar";
      return 1;
    }

    /* set the modified attributes */
    temp_contact->set_modified_host_attributes(
        temp_contact->get_modified_host_attributes() | hattr);
    temp_contact->set_modified_service_attributes(
        temp_contact->get_modified_service_attributes() | sattr);

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeHostObjectCustomVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectCustomVar* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    // std::shared_ptr<engine::host> temp_host;
    host* temp_host{nullptr};
    std::string varname(request->varname());

    std::transform(varname.begin(), varname.end(), varname.begin(), ::toupper);
    host_map::const_iterator it_h(host::hosts.find(request->host_name()));
    if (it_h != host::hosts.end())
      temp_host = it_h->second.get();
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    map_customvar::iterator it(temp_host->custom_variables.find(varname));
    if (it == temp_host->custom_variables.end())
      temp_host->custom_variables[varname] =
          customvariable(request->varvalue());
    else
      it->second.update(request->varvalue());
    /* set the modified attributes and update the status of the object */
    temp_host->add_modified_attributes(MODATTR_CUSTOM_VARIABLE);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeServiceObjectCustomVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectCustomVar* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");
  if (request->service_desc().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "service description must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, &request]() -> int32_t {
    service* temp_service{nullptr};
    std::string varname(request->varname());

    std::transform(varname.begin(), varname.end(), varname.begin(), ::toupper);
    service_map::const_iterator it_s(service::services.find(
        {request->host_name(), request->service_desc()}));
    if (it_s != service::services.end())
      temp_service = it_s->second.get();
    if (temp_service == nullptr) {
      err = fmt::format("could not find service ('{}', '{}')",
                        request->host_name(), request->service_desc());
      return 1;
    }
    map_customvar::iterator it(temp_service->custom_variables.find(varname));
    if (it == temp_service->custom_variables.end())
      temp_service->custom_variables[varname] =
          customvariable(request->varvalue());
    else
      it->second.update(request->varvalue());
    temp_service->add_modified_attributes(MODATTR_CUSTOM_VARIABLE);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeContactObjectCustomVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectCustomVar* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->contact().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "contact must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    contact* temp_contact{nullptr};
    std::string varname(request->varname());

    std::transform(varname.begin(), varname.end(), varname.begin(), ::toupper);
    contact_map::iterator cnct_it = contact::contacts.find(request->contact());
    if (cnct_it != contact::contacts.end())
      temp_contact = cnct_it->second.get();
    if (temp_contact == nullptr) {
      err = fmt::format("could not find contact '{}'", request->contact());
      return 1;
    }
    map_customvar::iterator it(
        temp_contact->get_custom_variables().find(varname));
    if (it == temp_contact->get_custom_variables().end())
      temp_contact->get_custom_variables()[varname] =
          customvariable(request->varvalue());
    else
      it->second.update(request->varvalue());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Shutdown Program.
 *
 * @param context gRPC context
 * @param unused
 * @param response Command success
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ShutdownProgram(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  auto fn = std::packaged_task<int32_t(void)>([]() -> int32_t {
    exit(0);
    return 0;
  });

  command_manager::instance().enqueue(std::move(fn));

  return grpc::Status::OK;
}

#define HOST_METHOD_BEGIN                                                    \
  SPDLOG_LOGGER_DEBUG(external_command_logger, "{}({})", __FUNCTION__,       \
                      *request);                                             \
  auto host_info = get_host(*request);                                       \
  if (!host_info.second.empty()) {                                           \
    SPDLOG_LOGGER_ERROR(external_command_logger, "{}({}) : unknown host {}", \
                        __FUNCTION__, *request, host_info.second);           \
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,                \
                        host_info.second);                                   \
  }

#define SERV_METHOD_BEGIN                                                    \
  SPDLOG_LOGGER_DEBUG(external_command_logger, "{}({})", __FUNCTION__,       \
                      *request);                                             \
  auto serv_info = get_serv(*request);                                       \
  if (!serv_info.second.empty()) {                                           \
    SPDLOG_LOGGER_ERROR(external_command_logger, "{}({}) : unknown serv {}", \
                        __FUNCTION__, *request, serv_info.second);           \
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,                \
                        serv_info.second);                                   \
  }

::grpc::Status engine_impl::EnableHostAndChildNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::NameOrIdIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  HOST_METHOD_BEGIN
  commands::processing::wrapper_enable_host_and_child_notifications(
      host_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::DisableHostAndChildNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::NameOrIdIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  HOST_METHOD_BEGIN
  commands::processing::wrapper_disable_host_and_child_notifications(
      host_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::DisableHostNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::NameOrIdIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  HOST_METHOD_BEGIN
  disable_host_notifications(host_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::EnableHostNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::NameOrIdIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  HOST_METHOD_BEGIN
  enable_host_notifications(host_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::DisableNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty*,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  disable_all_notifications();
  return grpc::Status::OK;
}

::grpc::Status engine_impl::EnableNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty*,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  enable_all_notifications();
  return grpc::Status::OK;
}

::grpc::Status engine_impl::DisableServiceNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::ServiceIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  SERV_METHOD_BEGIN
  disable_service_notifications(serv_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::EnableServiceNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::ServiceIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  SERV_METHOD_BEGIN
  enable_service_notifications(serv_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::ChangeAnomalyDetectionSensitivity(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::ChangeServiceNumber* serv_and_value,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  SPDLOG_LOGGER_DEBUG(external_command_logger, "{}({})", __FUNCTION__,
                      serv_and_value->serv());
  auto serv_info = get_serv(serv_and_value->serv());
  if (!serv_info.second.empty()) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "{}({}) : unknown serv {}",
                        __FUNCTION__, serv_and_value->serv(), serv_info.second);
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, serv_info.second);
  }

  if (serv_info.first->get_service_type() != service_type::ANOMALY_DETECTION) {
    SPDLOG_LOGGER_ERROR(external_command_logger,
                        "{}({}) : {} is not an anomalydetection", __FUNCTION__,
                        serv_and_value->serv(), serv_info.second);
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, serv_info.second);
  }

  std::shared_ptr<anomalydetection> ano =
      std::static_pointer_cast<anomalydetection>(serv_info.first);

  if (serv_and_value->has_dval()) {
    ano->set_sensitivity(serv_and_value->dval());
    return grpc::Status::OK;
  }

  if (serv_and_value->has_intval()) {
    ano->set_sensitivity(serv_and_value->intval());
    return grpc::Status::OK;
  }
  SPDLOG_LOGGER_ERROR(external_command_logger, "{}({}) : no value provided",
                      __FUNCTION__, serv_and_value->serv());
  return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                      "no value provided");
}

/**
 * @brief find host ever by name or id
 *
 * @param host_info
 * @return std::shared_ptr<com::centreon::engine::host>
 */
std::pair<std::shared_ptr<com::centreon::engine::host>, std::string>
engine_impl::get_host(
    const ::com::centreon::engine::NameOrIdIdentifier& host_info) {
  std::string err;
  switch (host_info.identifier_case()) {
    case NameOrIdIdentifier::kName: {
      /* get the host */
      auto ithostname = host::hosts.find(host_info.name());
      if (ithostname != host::hosts.end())
        return std::make_pair(ithostname->second, err);
      else {
        err = fmt::format("could not find host '{}'", host_info.name());
        return std::make_pair(std::shared_ptr<com::centreon::engine::host>(),
                              err);
      }
    } break;
    case NameOrIdIdentifier::kId: {
      /* get the host */
      auto ithostid = host::hosts_by_id.find(host_info.id());
      if (ithostid != host::hosts_by_id.end())
        return std::make_pair(ithostid->second, err);
      else {
        err = fmt::format("could not find host {}", host_info.id());
        return std::make_pair(std::shared_ptr<com::centreon::engine::host>(),
                              err);
      }
    } break;
    default: {
      err = fmt::format("could not find identifier, you should inform a host");
      return std::make_pair(std::shared_ptr<com::centreon::engine::host>(),
                            err);
    }
  }
}

std::pair<std::shared_ptr<com::centreon::engine::service>,
          std::string /*error*/>
engine_impl::get_serv(
    const ::com::centreon::engine::ServiceIdentifier& serv_info) {
  std::string err;

  /* checking identifier sesrname (by names or by ids) */
  switch (serv_info.identifier_case()) {
    case ServiceIdentifier::kNames: {
      const PairNamesIdentifier& names = serv_info.names();
      /* get the service */
      auto itservicenames = service::services.find(
          std::make_pair(names.host_name(), names.service_name()));
      if (itservicenames != service::services.end())
        return std::make_pair(itservicenames->second, err);
      else {
        err = fmt::format("could not find service ('{}', '{}')",
                          names.host_name(), names.service_name());
        return std::make_pair(std::shared_ptr<com::centreon::engine::service>(),
                              err);
      }
    } break;
    case ServiceIdentifier::kIds: {
      const PairIdsIdentifier& ids = serv_info.ids();
      /* get the service */
      auto itserviceids = service::services_by_id.find(
          std::make_pair(ids.host_id(), ids.service_id()));
      if (itserviceids != service::services_by_id.end())
        return std::make_pair(itserviceids->second, err);
      else {
        err = fmt::format("could not find service ({}, {})", ids.host_id(),
                          ids.service_id());
        return std::make_pair(std::shared_ptr<com::centreon::engine::service>(),
                              err);
      }
    } break;
    default: {
      err = "could not find identifier, you should inform a service";
      return std::make_pair(std::shared_ptr<com::centreon::engine::service>(),
                            err);
    }
  }
}

/**
 * @brief get log levels and infos
 *
 * @param context
 * @param request
 * @param response
 * @return ::grpc::Status
 */
::grpc::Status engine_impl::GetLogInfo(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    ::com::centreon::engine::LogInfo* response) {
  std::vector<std::shared_ptr<spdlog::logger>> loggers;

  spdlog::apply_all([&loggers](const std::shared_ptr<spdlog::logger>& logger) {
    loggers.push_back(logger);
  });

  response->set_log_file(log_v2::instance().filename());
  response->set_log_flush_period(log_v2::instance().flush_interval().count());
  auto levels = response->mutable_level();
  for (const auto& logger : loggers) {
    auto level = spdlog::level::to_string_view(logger->level());
    (*levels)[logger->name()] = std::string(level.data(), level.size());
  }
  return grpc::Status::OK;
}

grpc::Status engine_impl::SetLogLevel(grpc::ServerContext* context
                                      [[maybe_unused]],
                                      const LogLevel* request,
                                      ::google::protobuf::Empty*) {
  const std::string& logger_name{request->logger()};
  std::shared_ptr<spdlog::logger> logger = spdlog::get(logger_name);
  if (!logger) {
    std::string err_detail =
        fmt::format("The '{}' logger does not exist", logger_name);
    SPDLOG_LOGGER_ERROR(external_command_logger, err_detail);
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err_detail);
  } else {
    logger->set_level(spdlog::level::level_enum(request->level()));
    return grpc::Status::OK;
  }
}

grpc::Status engine_impl::SetLogFlushPeriod(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const LogFlushPeriod* request,
                                            ::google::protobuf::Empty*) {
  // first get all log_v2 objects
  log_v2::instance().set_flush_interval(request->period());
  return grpc::Status::OK;
}

/**
 * @brief get stats of the process (cpu, memory...)
 *
 * @param context
 * @param request
 * @param response
 * @return grpc::Status
 */
grpc::Status engine_impl::GetProcessStats(
    grpc::ServerContext* context [[maybe_unused]],
    const google::protobuf::Empty*,
    com::centreon::common::pb_process_stat* response) {
  try {
    com::centreon::common::process_stat stat(getpid());
    stat.to_protobuff(*response);
  } catch (const boost::exception& e) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "fail to get process info: {}",
                        boost::diagnostic_information(e));

    return grpc::Status(grpc::StatusCode::INTERNAL,
                        boost::diagnostic_information(e));
  }
  return grpc::Status::OK;
}

/**
 * @brief send a bench event across brokers network
 *
 * @param context
 * @param request
 * @param response
 * @return grpc::Status
 */
grpc::Status engine_impl::SendBench(
    grpc::ServerContext* context [[maybe_unused]],
    const com::centreon::engine::BenchParam* request,
    google::protobuf::Empty* response [[maybe_unused]]) {
  std::chrono::system_clock::time_point client_ts =
      std::chrono::system_clock::time_point::min();

  if (request->ts().seconds() > 0) {
    client_ts = common::google_ts_to_time_point(request->ts());
  }

  broker_bench(request->id(), client_ts);
  return grpc::Status::OK;
}
