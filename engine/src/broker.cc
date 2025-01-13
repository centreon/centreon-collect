/**
 * Copyright 2009-2025 Centreon
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
#include "com/centreon/engine/broker.hh"
#include <absl/strings/str_split.h>
#include <unistd.h>
#include "bbdo/neb.pb.h"
#include "broker/core/bbdo/internal.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
#include "com/centreon/broker/neb/comment.hh"
#include "com/centreon/broker/neb/custom_variable.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/host_check.hh"
#include "com/centreon/broker/neb/host_group.hh"
#include "com/centreon/broker/neb/host_group_member.hh"
#include "com/centreon/broker/neb/host_parent.hh"
#include "com/centreon/broker/neb/instance_configuration.hh"
#include "com/centreon/broker/neb/instance_status.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/log_entry.hh"
#include "com/centreon/broker/neb/service.hh"
#include "com/centreon/broker/neb/service_check.hh"
#include "com/centreon/broker/neb/service_group.hh"
#include "com/centreon/broker/neb/service_group_member.hh"
#include "com/centreon/common/time.hh"
#include "com/centreon/common/utf8.hh"
#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
#include "com/centreon/engine/flapping.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/nebcallbacks.hh"
#include "com/centreon/engine/nebstructs.hh"
#include "com/centreon/engine/sehandlers.hh"
#include "com/centreon/engine/severity.hh"
#include "com/centreon/engine/string.hh"
#include "common.h"

using namespace com::centreon::broker;
using namespace com::centreon::engine;
using namespace com::centreon;

template <typename R>
static void forward_acknowledgement(const char* author_name,
                                    const char* comment_data,
                                    int subtype,
                                    bool notify_contacts,
                                    bool persistent_comment,
                                    R* resource) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger,
                      "callbacks: generating acknowledgement event");

  try {
    // In/Out variables.
    auto ack = std::make_shared<neb::acknowledgement>();

    if constexpr (std::is_same_v<R, engine::service>) {
      ack->acknowledgement_type = short(acknowledgement_resource_type::SERVICE);
      ack->service_id = resource->service_id();
    } else {
      ack->acknowledgement_type = short(acknowledgement_resource_type::HOST);
    }
    assert(resource->host_id());
    ack->host_id = resource->host_id();
    if (author_name)
      ack->author = common::check_string_utf8(author_name);
    if (comment_data)
      ack->comment = common::check_string_utf8(comment_data);
    ack->entry_time = time(nullptr);
    ack->poller_id = cbm->poller_id();
    ack->is_sticky = subtype == AckType::STICKY;
    ack->notify_contacts = notify_contacts;
    ack->persistent_comment = persistent_comment;
    ack->state = resource->get_current_state();

    // Store acknowledgement.
    cbm->add_acknowledgement(ack);

    // Send event.
    cbm->write(ack);
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(
        neb_logger,
        "callbacks: error occurred while generating acknowledgement event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
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
template <typename R>
static void forward_pb_acknowledgement(const char* author_name,
                                       const char* comment_data,
                                       int subtype,
                                       bool notify_contacts,
                                       bool persistent_comment,
                                       R* resource) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger,
                      "callbacks: generating pb acknowledgement event");

  // In/Out variables.
  auto ack{std::make_shared<neb::pb_acknowledgement>()};
  auto& ack_obj = ack->mut_obj();

  if constexpr (std::is_same_v<R, com::centreon::engine::service>) {
    ack_obj.set_type(
        com::centreon::broker::Acknowledgement_ResourceType_SERVICE);
    ack_obj.set_service_id(resource->service_id());
  } else {
    ack_obj.set_type(com::centreon::broker::Acknowledgement_ResourceType_HOST);
  }
  assert(resource->host_id());
  ack_obj.set_host_id(resource->host_id());
  // Fill output var.
  if (author_name)
    ack_obj.set_author(common::check_string_utf8(author_name));
  if (comment_data)
    ack_obj.set_comment_data(common::check_string_utf8(comment_data));
  ack_obj.set_entry_time(time(nullptr));
  ack_obj.set_instance_id(cbm->poller_id());
  ack_obj.set_sticky(subtype == AckType::STICKY);
  ack_obj.set_notify_contacts(notify_contacts);
  ack_obj.set_persistent_comment(persistent_comment);
  ack_obj.set_state(resource->get_current_state());

  cbm->add_acknowledgement(ack);

  // Send event.
  cbm->write(ack);
}

/**
 *  Send acknowledgement data to broker.
 *
 *  @param[in] data                 A pointer to a service or host class.
 *  @param[in] ack_author           Author.
 *  @param[in] ack_data             Acknowledgement text.
 *  @param[in] subtype              Subtype to know if the acknowledgement is
 * sticky.
 *  @param[in] notify_contacts      Should we notify contacts.
 *  @param[in] persistent_comment   Persistent comment
 */
template <typename R>
void broker_acknowledgement_data(R* data,
                                 const char* ack_author,
                                 const char* ack_data,
                                 int subtype,
                                 bool notify_contacts,
                                 bool persistent_comment) {
  // Config check.
  if (!(pb_config.event_broker_options() & BROKER_ACKNOWLEDGEMENT_DATA))
    return;

  if (cbm->use_protobuf())
    forward_pb_acknowledgement(ack_author, ack_data, subtype, notify_contacts,
                               persistent_comment, data);
  else
    forward_acknowledgement(ack_author, ack_data, subtype, notify_contacts,
                            persistent_comment, data);
}

template void broker_acknowledgement_data<com::centreon::engine::service>(
    com::centreon::engine::service* data,
    const char* ack_author,
    const char* ack_data,
    int subtype,
    bool notify_contacts,
    bool persistent_comment);

template void broker_acknowledgement_data<com::centreon::engine::host>(
    com::centreon::engine::host* data,
    const char* ack_author,
    const char* ack_data,
    int subtype,
    bool notify_contacts,
    bool persistent_comment);

/**
 * @brief Send adaptive severity updates to broker.
 *
 * @param type      Type.
 * @param data      Target severity.
 */
void broker_adaptive_severity_data(int type, engine::severity* es) {
  /* Config check. */
  if (!(pb_config.event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;

  SPDLOG_LOGGER_DEBUG(neb_logger,
                      "callbacks: generating protobuf severity event");

  auto s{std::make_shared<neb::pb_severity>()};
  com::centreon::broker::Severity& sv = s.get()->mut_obj();
  switch (type) {
    case NEBTYPE_SEVERITY_ADD:
      SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: new severity");
      sv.set_action(com::centreon::broker::Severity_Action_ADD);
      break;
    case NEBTYPE_SEVERITY_DELETE:
      SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: removed severity");
      sv.set_action(com::centreon::broker::Severity_Action_DELETE);
      break;
    case NEBTYPE_SEVERITY_UPDATE:
      SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: modified severity");
      sv.set_action(com::centreon::broker::Severity_Action_MODIFY);
      break;
    default:
      SPDLOG_LOGGER_ERROR(neb_logger,
                          "callbacks: protobuf severity event action must be "
                          "among ADD, MODIFY "
                          "or DELETE");
      return;
  }
  sv.set_id(es->id());
  sv.set_poller_id(cbm->poller_id());
  sv.set_level(es->level());
  sv.set_icon_id(es->icon_id());
  sv.set_name(es->name());
  sv.set_type(static_cast<com::centreon::broker::Severity_Type>(es->type()));

  // Send event(s).
  cbm->write(s);
}

/**
 * @brief Send adaptive tag updates to broker.
 *
 * @param type      Type.
 * @param data      Target tag.
 */
void broker_adaptive_tag_data(int type, engine::tag* et) {
  /* Config check. */
  if (!(pb_config.event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;

  /* Make callbacks. */
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating protobuf tag event");

  auto t{std::make_shared<neb::pb_tag>()};
  com::centreon::broker::Tag& tg = t.get()->mut_obj();
  switch (type) {
    case NEBTYPE_TAG_ADD:
      SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: new tag");
      tg.set_action(com::centreon::broker::Tag_Action_ADD);
      break;
    case NEBTYPE_TAG_DELETE:
      SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: removed tag");
      tg.set_action(com::centreon::broker::Tag_Action_DELETE);
      break;
    case NEBTYPE_TAG_UPDATE:
      SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: modified tag");
      tg.set_action(com::centreon::broker::Tag_Action_MODIFY);
      break;
    default:
      SPDLOG_LOGGER_ERROR(
          neb_logger,
          "callbacks: protobuf tag event action must be among ADD, MODIFY "
          "or DELETE");
      return;
  }
  tg.set_id(et->id());
  tg.set_poller_id(cbm->poller_id());
  switch (et->type()) {
    case engine::tag::hostcategory:
      tg.set_type(com::centreon::broker::HOSTCATEGORY);
      break;
    case engine::tag::servicecategory:
      tg.set_type(com::centreon::broker::SERVICECATEGORY);
      break;
    case engine::tag::hostgroup:
      tg.set_type(com::centreon::broker::HOSTGROUP);
      break;
    case engine::tag::servicegroup:
      tg.set_type(com::centreon::broker::SERVICEGROUP);
      break;
    default:
      SPDLOG_LOGGER_ERROR(
          neb_logger,
          "callbacks: protobuf tag event type must be among HOSTCATEGORY, "
          "SERVICECATEGORY, HOSTGROUP or SERVICEGROUP");
      return;
  }
  tg.set_name(et->name());

  // Send event(t).
  cbm->write(t);
}

static void forward_host(int type,
                         int flags,
                         uint64_t modified_attribute [[maybe_unused]],
                         const engine::host* h) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating host event");

  // In/Out variables.
  if (flags & NEBATTR_BBDO3_ONLY)
    return;
  auto my_host = std::make_shared<neb::host>();

  // Set host parameters.
  my_host->acknowledged = h->problem_has_been_acknowledged();
  my_host->acknowledgement_type = h->get_acknowledgement();
  if (!h->get_action_url().empty())
    my_host->action_url = common::check_string_utf8(h->get_action_url());
  my_host->active_checks_enabled = h->active_checks_enabled();
  if (!h->get_address().empty())
    my_host->address = common::check_string_utf8(h->get_address());
  if (!h->get_alias().empty())
    my_host->alias = common::check_string_utf8(h->get_alias());
  my_host->check_freshness = h->check_freshness_enabled();
  if (!h->check_command().empty())
    my_host->check_command = common::check_string_utf8(h->check_command());
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
    my_host->display_name = common::check_string_utf8(h->get_display_name());
  my_host->enabled = (type != NEBTYPE_HOST_DELETE);
  if (!h->event_handler().empty())
    my_host->event_handler = common::check_string_utf8(h->event_handler());
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
    my_host->host_name = common::check_string_utf8(h->name());
  if (!h->get_icon_image().empty())
    my_host->icon_image = common::check_string_utf8(h->get_icon_image());
  if (!h->get_icon_image_alt().empty())
    my_host->icon_image_alt =
        common::check_string_utf8(h->get_icon_image_alt());
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
    my_host->notes = common::check_string_utf8(h->get_notes());
  if (!h->get_notes_url().empty())
    my_host->notes_url = common::check_string_utf8(h->get_notes_url());
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
    my_host->output = common::check_string_utf8(h->get_plugin_output());
    my_host->output.append("\n");
  }
  if (!h->get_long_plugin_output().empty())
    my_host->output.append(
        common::check_string_utf8(h->get_long_plugin_output()));
  my_host->passive_checks_enabled = h->passive_checks_enabled();
  my_host->percent_state_change = h->get_percent_state_change();
  if (!h->get_perf_data().empty())
    my_host->perf_data = common::check_string_utf8(h->get_perf_data());
  my_host->poller_id = cbm->poller_id();
  my_host->retain_nonstatus_information = h->get_retain_nonstatus_information();
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
        common::check_string_utf8(h->get_statusmap_image());
  my_host->timezone = h->get_timezone();

  // Find host ID.
  uint64_t host_id = engine::get_host_id(my_host->host_name);
  if (host_id != 0) {
    my_host->host_id = host_id;

    // Send host event.
    SPDLOG_LOGGER_DEBUG(
        neb_logger, "callbacks:  new host {} ('{}') on instance {}",
        my_host->host_id, my_host->host_name, my_host->poller_id);
    cbm->write(my_host);

    /* No need to send this service custom variables changes, custom
     * variables are managed in a different loop. */
  } else
    SPDLOG_LOGGER_ERROR(neb_logger,
                        "callbacks: host '{}' has no ID (yet) defined",
                        (!h->name().empty() ? h->name() : "(unknown)"));
}

static void forward_pb_host(int type,
                            int flags [[maybe_unused]],
                            uint64_t modified_attribute,
                            const engine::host* eh) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger,
                      "callbacks: generating pb host event protobuf");

  if (type == NEBTYPE_ADAPTIVEHOST_UPDATE &&
      modified_attribute != MODATTR_ALL) {
    std::shared_ptr<neb::pb_adaptive_host> h;
    // auto h =
    // std::make_shared<neb::pb_adaptive_host>();
    auto& hst = h->mut_obj();
    if (modified_attribute & MODATTR_NOTIFICATIONS_ENABLED)
      hst.set_notify(eh->get_notifications_enabled());
    else if (modified_attribute & MODATTR_ACTIVE_CHECKS_ENABLED) {
      hst.set_active_checks(eh->active_checks_enabled());
      hst.set_should_be_scheduled(eh->get_should_be_scheduled());
    } else if (modified_attribute & MODATTR_PASSIVE_CHECKS_ENABLED)
      hst.set_passive_checks(eh->passive_checks_enabled());
    else if (modified_attribute & MODATTR_EVENT_HANDLER_ENABLED)
      hst.set_event_handler_enabled(eh->event_handler_enabled());
    else if (modified_attribute & MODATTR_FLAP_DETECTION_ENABLED)
      hst.set_flap_detection(eh->flap_detection_enabled());
    else if (modified_attribute & MODATTR_OBSESSIVE_HANDLER_ENABLED)
      hst.set_obsess_over_host(eh->obsess_over());
    else if (modified_attribute & MODATTR_EVENT_HANDLER_COMMAND)
      hst.set_event_handler(common::check_string_utf8(eh->event_handler()));
    else if (modified_attribute & MODATTR_CHECK_COMMAND)
      hst.set_check_command(common::check_string_utf8(eh->check_command()));
    else if (modified_attribute & MODATTR_NORMAL_CHECK_INTERVAL)
      hst.set_check_interval(eh->check_interval());
    else if (modified_attribute & MODATTR_RETRY_CHECK_INTERVAL)
      hst.set_retry_interval(eh->retry_interval());
    else if (modified_attribute & MODATTR_MAX_CHECK_ATTEMPTS)
      hst.set_max_check_attempts(eh->max_check_attempts());
    else if (modified_attribute & MODATTR_FRESHNESS_CHECKS_ENABLED)
      hst.set_check_freshness(eh->check_freshness_enabled());
    else if (modified_attribute & MODATTR_CHECK_TIMEPERIOD)
      hst.set_check_period(eh->check_period());
    else if (modified_attribute & MODATTR_NOTIFICATION_TIMEPERIOD)
      hst.set_notification_period(eh->notification_period());
    else {
      SPDLOG_LOGGER_ERROR(neb_logger,
                          "callbacks: adaptive host not implemented.");
      assert(1 == 0);
    }

    uint64_t host_id = engine::get_host_id(eh->name());
    if (host_id != 0) {
      hst.set_host_id(host_id);

      // Send host event.
      SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks:  new host {} ('{}')",
                          hst.host_id(), eh->name());
      cbm->write(h);
    } else
      SPDLOG_LOGGER_ERROR(neb_logger,
                          "callbacks: host '{}' has no ID (yet) defined",
                          (!eh->name().empty() ? eh->name() : "(unknown)"));
  } else {
    auto h = std::make_shared<neb::pb_host>();
    auto& host = h->mut_obj();

    // Set host parameters.
    host.set_acknowledged(eh->problem_has_been_acknowledged());
    host.set_acknowledgement_type(eh->get_acknowledgement());
    if (!eh->get_action_url().empty())
      host.set_action_url(common::check_string_utf8(eh->get_action_url()));
    host.set_active_checks(eh->active_checks_enabled());
    if (!eh->get_address().empty())
      host.set_address(common::check_string_utf8(eh->get_address()));
    if (!eh->get_alias().empty())
      host.set_alias(common::check_string_utf8(eh->get_alias()));
    host.set_check_freshness(eh->check_freshness_enabled());
    if (!eh->check_command().empty())
      host.set_check_command(common::check_string_utf8(eh->check_command()));
    host.set_check_interval(eh->check_interval());
    if (!eh->check_period().empty())
      host.set_check_period(eh->check_period());
    host.set_check_type(static_cast<com::centreon::broker::Host_CheckType>(
        eh->get_check_type()));
    host.set_check_attempt(eh->get_current_attempt());
    host.set_state(static_cast<com::centreon::broker::Host_State>(
        eh->has_been_checked() ? eh->get_current_state()
                               : 4));  // Pending state.
    host.set_default_active_checks(eh->active_checks_enabled());
    host.set_default_event_handler_enabled(eh->event_handler_enabled());
    host.set_default_flap_detection(eh->flap_detection_enabled());
    host.set_default_notify(eh->get_notifications_enabled());
    host.set_default_passive_checks(eh->passive_checks_enabled());
    host.set_scheduled_downtime_depth(eh->get_scheduled_downtime_depth());
    if (!eh->get_display_name().empty())
      host.set_display_name(common::check_string_utf8(eh->get_display_name()));
    host.set_enabled(type != NEBTYPE_HOST_DELETE);
    if (!eh->event_handler().empty())
      host.set_event_handler(common::check_string_utf8(eh->event_handler()));
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
      host.set_name(common::check_string_utf8(eh->name()));
    if (!eh->get_icon_image().empty())
      host.set_icon_image(common::check_string_utf8(eh->get_icon_image()));
    if (!eh->get_icon_image_alt().empty())
      host.set_icon_image_alt(
          common::check_string_utf8(eh->get_icon_image_alt()));
    host.set_flapping(eh->get_is_flapping());
    host.set_last_check(eh->get_last_check());
    host.set_last_hard_state(static_cast<com::centreon::broker::Host_State>(
        eh->get_last_hard_state()));
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
      host.set_notes(common::check_string_utf8(eh->get_notes()));
    if (!eh->get_notes_url().empty())
      host.set_notes_url(common::check_string_utf8(eh->get_notes_url()));
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
      host.set_output(common::check_string_utf8(eh->get_plugin_output()));
    }
    if (!eh->get_long_plugin_output().empty())
      host.set_output(common::check_string_utf8(eh->get_long_plugin_output()));
    host.set_passive_checks(eh->passive_checks_enabled());
    host.set_percent_state_change(eh->get_percent_state_change());
    if (!eh->get_perf_data().empty())
      host.set_perfdata(common::check_string_utf8(eh->get_perf_data()));
    host.set_instance_id(cbm->poller_id());
    host.set_retain_nonstatus_information(
        eh->get_retain_nonstatus_information());
    host.set_retain_status_information(eh->get_retain_status_information());
    host.set_retry_interval(eh->retry_interval());
    host.set_should_be_scheduled(eh->get_should_be_scheduled());
    host.set_stalk_on_down(eh->get_stalk_on(engine::notifier::down));
    host.set_stalk_on_unreachable(
        eh->get_stalk_on(engine::notifier::unreachable));
    host.set_stalk_on_up(eh->get_stalk_on(engine::notifier::up));
    host.set_state_type(static_cast<com::centreon::broker::Host_StateType>(
        eh->has_been_checked() ? eh->get_state_type()
                               : engine::notifier::hard));
    if (!eh->get_statusmap_image().empty())
      host.set_statusmap_image(
          common::check_string_utf8(eh->get_statusmap_image()));
    host.set_timezone(eh->get_timezone());
    host.set_severity_id(eh->get_severity() ? eh->get_severity()->id() : 0);
    host.set_icon_id(eh->get_icon_id());
    for (auto& tg : eh->tags()) {
      com::centreon::broker::TagInfo* ti = host.mutable_tags()->Add();
      ti->set_id(tg->id());
      ti->set_type(static_cast<com::centreon::broker::TagType>(tg->type()));
    }

    // Find host ID.
    uint64_t host_id = engine::get_host_id(host.name());
    if (host_id != 0) {
      host.set_host_id(host_id);

      // Send host event.
      SPDLOG_LOGGER_DEBUG(neb_logger,
                          "callbacks:  new host {} ('{}') on instance {}",
                          host.host_id(), host.name(), host.instance_id());
      cbm->write(h);

      /* No need to send this service custom variables changes, custom
       * variables are managed in a different loop. */
    } else
      SPDLOG_LOGGER_ERROR(neb_logger,
                          "callbacks: host '{}' has no ID (yet) defined",
                          (!eh->name().empty() ? eh->name() : "(unknown)"));
  }
}

/**
 * @brief Send adaptive host updates to broker.
 *
 * @param type  NEBTYPE_HOST_ADD, NEBTYPE_HOST_DELETE, etc...
 * @param flags Used in one specific case, when data should be sent only in
 * bbdo3.
 * @param hst   the host to handle.
 * @param modattr modified attributes.
 */
void broker_adaptive_host_data(int type,
                               int flags,
                               host* hst,
                               uint64_t modattr) {
  // Config check.
  if (!(pb_config.event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;

  // Make callbacks.
  if (cbm->use_protobuf())
    forward_pb_host(type, flags, modattr, hst);
  else
    forward_host(type, flags, modattr, hst);
}

/**
 *  @brief Process service data.
 *
 *  This function is called by Engine when some service data is
 *  available.
 *
 *  @param[in] data          A pointer to a
 *                           nebstruct_adaptive_service_data containing
 *                           the service data.
 *
 *  @return 0 on success.
 */
static void forward_service(int type,
                            int flags,
                            uint64_t modified_attribute [[maybe_unused]],
                            const engine::service* s) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating service event");

  try {
    // In/Out variables.
    if (flags & NEBATTR_BBDO3_ONLY)
      return;
    auto my_service{std::make_shared<neb::service>()};

    // Fill output var.
    my_service->acknowledged = s->problem_has_been_acknowledged();
    my_service->acknowledgement_type = s->get_acknowledgement();
    if (!s->get_action_url().empty())
      my_service->action_url = common::check_string_utf8(s->get_action_url());
    my_service->active_checks_enabled = s->active_checks_enabled();
    if (!s->check_command().empty())
      my_service->check_command = common::check_string_utf8(s->check_command());
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
          common::check_string_utf8(s->get_display_name());
    my_service->enabled = type != NEBTYPE_SERVICE_DELETE;
    if (!s->event_handler().empty())
      my_service->event_handler = common::check_string_utf8(s->event_handler());
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
      my_service->host_name = common::check_string_utf8(s->get_hostname());
    if (!s->get_icon_image().empty())
      my_service->icon_image = common::check_string_utf8(s->get_icon_image());
    if (!s->get_icon_image_alt().empty())
      my_service->icon_image_alt =
          common::check_string_utf8(s->get_icon_image_alt());
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
      my_service->notes = common::check_string_utf8(s->get_notes());
    if (!s->get_notes_url().empty())
      my_service->notes_url = common::check_string_utf8(s->get_notes_url());
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
      my_service->output = common::check_string_utf8(s->get_plugin_output());
      my_service->output.append("\n");
    }
    if (!s->get_long_plugin_output().empty())
      my_service->output.append(
          common::check_string_utf8(s->get_long_plugin_output()));
    my_service->passive_checks_enabled = s->passive_checks_enabled();
    my_service->percent_state_change = s->get_percent_state_change();
    if (!s->get_perf_data().empty())
      my_service->perf_data = common::check_string_utf8(s->get_perf_data());
    my_service->retain_nonstatus_information =
        s->get_retain_nonstatus_information();
    my_service->retain_status_information = s->get_retain_status_information();
    my_service->retry_interval = s->retry_interval();
    if (!s->description().empty())
      my_service->service_description =
          common::check_string_utf8(s->description());
    my_service->should_be_scheduled = s->get_should_be_scheduled();
    my_service->stalk_on_critical = s->get_stalk_on(engine::notifier::critical);
    my_service->stalk_on_ok = s->get_stalk_on(engine::notifier::ok);
    my_service->stalk_on_unknown = s->get_stalk_on(engine::notifier::unknown);
    my_service->stalk_on_warning = s->get_stalk_on(engine::notifier::warning);
    my_service->state_type =
        (s->has_been_checked() ? s->get_state_type() : engine::notifier::hard);

    // Search host ID and service ID.
    std::pair<uint64_t, uint64_t> p;
    p = engine::get_host_and_service_id(s->get_hostname(), s->description());
    my_service->host_id = p.first;
    my_service->service_id = p.second;
    if (my_service->host_id && my_service->service_id) {
      // Send service event.
      SPDLOG_LOGGER_DEBUG(neb_logger,
                          "callbacks: new service {} ('{}') on host {}",
                          my_service->service_id,
                          my_service->service_description, my_service->host_id);
      cbm->write(my_service);

      /* No need to send this service custom variables changes, custom
       * variables are managed in a different loop. */
    } else
      SPDLOG_LOGGER_ERROR(
          neb_logger,
          "callbacks: service has no host ID or no service ID (yet) (host "
          "'{}', service '{}')",
          (!s->get_hostname().empty() ? my_service->host_name : "(unknown)"),
          (!s->description().empty() ? my_service->service_description
                                     : "(unknown)"));
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
}

template <typename SrvStatus>
static void fill_service_type(SrvStatus& ss,
                              const com::centreon::engine::service* es) {
  switch (es->get_service_type()) {
    case com::centreon::engine::service_type::METASERVICE: {
      ss.set_type(com::centreon::broker::METASERVICE);
      uint64_t iid;
      if (absl::SimpleAtoi(es->description().c_str() + 5, &iid))
        ss.set_internal_id(iid);
      else {
        SPDLOG_LOGGER_ERROR(
            neb_logger,
            "callbacks: service ('{}', '{}') looks like a meta-service but "
            "its name is malformed",
            es->get_hostname(), es->description());
      }
    } break;
    case com::centreon::engine::service_type::BA: {
      ss.set_type(com::centreon::broker::BA);
      uint64_t iid;
      if (absl::SimpleAtoi(es->description().c_str() + 3, &iid))
        ss.set_internal_id(iid);
      else {
        SPDLOG_LOGGER_ERROR(
            neb_logger,
            "callbacks: service ('{}', '{}') looks like a business-activity "
            "but its name is malformed",
            es->get_hostname(), es->description());
      }
    } break;
    case com::centreon::engine::service_type::ANOMALY_DETECTION:
      ss.set_type(com::centreon::broker::ANOMALY_DETECTION);
      {
        const com::centreon::engine::anomalydetection* ad =
            static_cast<const com::centreon::engine::anomalydetection*>(es);
        ss.set_internal_id(ad->get_internal_id());
      }
      break;
    default:
      ss.set_type(com::centreon::broker::SERVICE);
      break;
  }
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
static void forward_pb_service(int type,
                               int flags [[maybe_unused]],
                               uint64_t modified_attribute,
                               const engine::service* es) {
  SPDLOG_LOGGER_DEBUG(neb_logger,
                      "callbacks: generating pb service event protobuf");

  SPDLOG_LOGGER_TRACE(neb_logger, "modified_attribute = {}",
                      modified_attribute);
  if (type == NEBTYPE_ADAPTIVESERVICE_UPDATE &&
      modified_attribute != MODATTR_ALL) {
    auto s{std::make_shared<neb::pb_adaptive_service>()};
    bool done = false;
    com::centreon::broker::AdaptiveService& srv = s.get()->mut_obj();
    if (modified_attribute & MODATTR_NOTIFICATIONS_ENABLED) {
      srv.set_notify(es->get_notifications_enabled());
      done = true;
    }
    if (modified_attribute & MODATTR_ACTIVE_CHECKS_ENABLED) {
      srv.set_active_checks(es->active_checks_enabled());
      srv.set_should_be_scheduled(es->get_should_be_scheduled());
      done = true;
    }
    if (modified_attribute & MODATTR_PASSIVE_CHECKS_ENABLED) {
      srv.set_passive_checks(es->passive_checks_enabled());
      done = true;
    }
    if (modified_attribute & MODATTR_EVENT_HANDLER_ENABLED) {
      srv.set_event_handler_enabled(es->event_handler_enabled());
      done = true;
    }
    if (modified_attribute & MODATTR_FLAP_DETECTION_ENABLED) {
      srv.set_flap_detection_enabled(es->flap_detection_enabled());
      done = true;
    }
    if (modified_attribute & MODATTR_OBSESSIVE_HANDLER_ENABLED) {
      srv.set_obsess_over_service(es->obsess_over());
      done = true;
    }
    if (modified_attribute & MODATTR_EVENT_HANDLER_COMMAND) {
      srv.set_event_handler(common::check_string_utf8(es->event_handler()));
      done = true;
    }
    if (modified_attribute & MODATTR_CHECK_COMMAND) {
      srv.set_check_command(common::check_string_utf8(es->check_command()));
      done = true;
    }
    if (modified_attribute & MODATTR_NORMAL_CHECK_INTERVAL) {
      srv.set_check_interval(es->check_interval());
      done = true;
    }
    if (modified_attribute & MODATTR_RETRY_CHECK_INTERVAL) {
      srv.set_retry_interval(es->retry_interval());
      done = true;
    }
    if (modified_attribute & MODATTR_MAX_CHECK_ATTEMPTS) {
      srv.set_max_check_attempts(es->max_check_attempts());
      done = true;
    }
    if (modified_attribute & MODATTR_FRESHNESS_CHECKS_ENABLED) {
      srv.set_check_freshness(es->check_freshness_enabled());
      done = true;
    }
    if (modified_attribute & MODATTR_CHECK_TIMEPERIOD) {
      srv.set_check_period(es->check_period());
      done = true;
    }
    if (modified_attribute & MODATTR_NOTIFICATION_TIMEPERIOD) {
      srv.set_notification_period(es->notification_period());
      done = true;
    }
    if (!done) {
      SPDLOG_LOGGER_ERROR(
          neb_logger, "callbacks: adaptive service field {} not implemented.",
          modified_attribute);
    }
    std::pair<uint64_t, uint64_t> p{
        engine::get_host_and_service_id(es->get_hostname(), es->description())};
    if (p.first && p.second) {
      srv.set_host_id(p.first);
      srv.set_service_id(p.second);
      // Send service event.
      SPDLOG_LOGGER_DEBUG(neb_logger,
                          "callbacks: new service {} ('{}') on host {}",
                          srv.service_id(), es->description(), srv.host_id());
      cbm->write(s);

      /* No need to send this service custom variables changes, custom
       * variables are managed in a different loop. */
    } else
      SPDLOG_LOGGER_ERROR(
          neb_logger,
          "callbacks: service has no host ID or no service ID (yet) (host "
          "'{}', service '{}')",
          !es->get_hostname().empty() ? es->get_hostname() : "(unknown)",
          !es->description().empty() ? es->description() : "(unknown)");
  } else {
    auto s{std::make_shared<neb::pb_service>()};
    com::centreon::broker::Service& srv = s.get()->mut_obj();

    // Fill output var.
    srv.set_acknowledged(es->problem_has_been_acknowledged());
    srv.set_acknowledgement_type(es->get_acknowledgement());
    if (!es->get_action_url().empty())
      srv.set_action_url(common::check_string_utf8(es->get_action_url()));
    srv.set_active_checks(es->active_checks_enabled());
    if (!es->check_command().empty())
      srv.set_check_command(common::check_string_utf8(es->check_command()));
    srv.set_check_freshness(es->check_freshness_enabled());
    srv.set_check_interval(es->check_interval());
    if (!es->check_period().empty())
      srv.set_check_period(es->check_period());
    srv.set_check_type(static_cast<com::centreon::broker::Service_CheckType>(
        es->get_check_type()));
    srv.set_check_attempt(es->get_current_attempt());
    srv.set_state(static_cast<com::centreon::broker::Service_State>(
        es->has_been_checked() ? es->get_current_state()
                               : 4));  // Pending state.
    srv.set_default_active_checks(es->active_checks_enabled());
    srv.set_default_event_handler_enabled(es->event_handler_enabled());
    srv.set_default_flap_detection(es->flap_detection_enabled());
    srv.set_default_notify(es->get_notifications_enabled());
    srv.set_default_passive_checks(es->passive_checks_enabled());
    srv.set_scheduled_downtime_depth(es->get_scheduled_downtime_depth());
    if (!es->get_display_name().empty())
      srv.set_display_name(common::check_string_utf8(es->get_display_name()));
    srv.set_enabled(type != NEBTYPE_SERVICE_DELETE);
    if (!es->event_handler().empty())
      srv.set_event_handler(common::check_string_utf8(es->event_handler()));
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
    if (!es->description().empty())
      srv.set_description(common::check_string_utf8(es->description()));
    if (!es->get_hostname().empty()) {
      std::string name{common::check_string_utf8(es->get_hostname())};
      *srv.mutable_host_name() = std::move(name);
    }
    fill_service_type(srv, es);

    if (!es->get_icon_image().empty())
      *srv.mutable_icon_image() =
          common::check_string_utf8(es->get_icon_image());
    if (!es->get_icon_image_alt().empty())
      *srv.mutable_icon_image_alt() =
          common::check_string_utf8(es->get_icon_image_alt());
    srv.set_flapping(es->get_is_flapping());
    srv.set_is_volatile(es->get_is_volatile());
    srv.set_last_check(es->get_last_check());
    srv.set_last_hard_state(static_cast<com::centreon::broker::Service_State>(
        es->get_last_hard_state()));
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
      srv.set_notes(common::check_string_utf8(es->get_notes()));
    if (!es->get_notes_url().empty())
      *srv.mutable_notes_url() = common::check_string_utf8(es->get_notes_url());
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
          common::check_string_utf8(es->get_plugin_output());
    if (!es->get_long_plugin_output().empty())
      *srv.mutable_long_output() =
          common::check_string_utf8(es->get_long_plugin_output());
    srv.set_passive_checks(es->passive_checks_enabled());
    srv.set_percent_state_change(es->get_percent_state_change());
    if (!es->get_perf_data().empty())
      *srv.mutable_perfdata() = common::check_string_utf8(es->get_perf_data());
    srv.set_retain_nonstatus_information(
        es->get_retain_nonstatus_information());
    srv.set_retain_status_information(es->get_retain_status_information());
    srv.set_retry_interval(es->retry_interval());
    srv.set_should_be_scheduled(es->get_should_be_scheduled());
    srv.set_stalk_on_critical(es->get_stalk_on(engine::notifier::critical));
    srv.set_stalk_on_ok(es->get_stalk_on(engine::notifier::ok));
    srv.set_stalk_on_unknown(es->get_stalk_on(engine::notifier::unknown));
    srv.set_stalk_on_warning(es->get_stalk_on(engine::notifier::warning));
    srv.set_state_type(static_cast<com::centreon::broker::Service_StateType>(
        es->has_been_checked() ? es->get_state_type()
                               : engine::notifier::hard));
    srv.set_severity_id(es->get_severity() ? es->get_severity()->id() : 0);
    srv.set_icon_id(es->get_icon_id());

    for (auto& tg : es->tags()) {
      com::centreon::broker::TagInfo* ti = srv.mutable_tags()->Add();
      ti->set_id(tg->id());
      ti->set_type(static_cast<com::centreon::broker::TagType>(tg->type()));
    }

    // Search host ID and service ID.
    std::pair<uint64_t, uint64_t> p;
    p = engine::get_host_and_service_id(es->get_hostname(), es->description());
    srv.set_host_id(p.first);
    srv.set_service_id(p.second);
    if (srv.host_id() && srv.service_id())
      SPDLOG_LOGGER_DEBUG(neb_logger,
                          "callbacks: service ({}, {}) has a severity id {}",
                          srv.host_id(), srv.service_id(), srv.severity_id());
    if (srv.host_id() && srv.service_id()) {
      // Send service event.
      SPDLOG_LOGGER_DEBUG(neb_logger,
                          "callbacks: new service {} ('{}') on host {}",
                          srv.service_id(), srv.description(), srv.host_id());
      cbm->write(s);

      /* No need to send this service custom variables changes, custom
       * variables are managed in a different loop. */
    } else
      SPDLOG_LOGGER_ERROR(
          neb_logger,
          "callbacks: service has no host ID or no service ID (yet) (host "
          "'{}', service '{}')",
          (!es->get_hostname().empty() ? srv.host_name() : "(unknown)"),
          (!es->description().empty() ? srv.description() : "(unknown)"));
  }
}

/**
 *  Sends adaptive service updates to broker.
 *
 *  @param[in] type         Type.
 *  @param[in] flags        Flags.
 *  @param[in] attr         Attributes.
 *  @param[in] svc          Target service.
 *  @param[in] modattr      Global service modified attributes.
 */
void broker_adaptive_service_data(int type,
                                  int flags,
                                  engine::service* svc,
                                  unsigned long modattr) {
  // Config check.
  if (!(pb_config.event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;

  // Make callbacks.
  if (cbm->use_protobuf())
    forward_service(type, flags, modattr, svc);
  else
    forward_pb_service(type, flags, modattr, svc);
}

static void forward_comment(int type,
                            com::centreon::engine::comment::type comment_type,
                            com::centreon::engine::comment::e_type entry_type,
                            uint64_t host_id,
                            uint64_t service_id,
                            time_t entry_time,
                            const char* author_name,
                            const char* comment_text,
                            bool persistent,
                            com::centreon::engine::comment::src source,
                            bool expires,
                            time_t expire_time,
                            uint64_t comment_id) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating comment event");

  try {
    // In/Out variables.
    auto comment{std::make_shared<neb::comment>()};

    // Fill output var.
    if (author_name)
      comment->author = common::check_string_utf8(author_name);
    if (comment_text)
      comment->data = common::check_string_utf8(comment_text);
    comment->comment_type = comment_type;
    if (NEBTYPE_COMMENT_DELETE == type)
      comment->deletion_time = time(nullptr);
    comment->entry_time = entry_time;
    comment->entry_type = entry_type;
    if (comment->entry_type == 4)
      neb_logger->debug(
          "callbacks: comment about acknowledgement entry_time:{} - "
          "deletion_time:{} - host_id:{} - service_id:{}",
          comment->entry_time, comment->deletion_time, comment->host_id,
          comment->service_id);
    comment->expire_time = expire_time;
    comment->expires = expires;
    if (service_id) {
      comment->host_id = host_id;
      comment->service_id = service_id;
      if (!comment->host_id)
        throw exceptions::msg_fmt(
            "comment created from a service with host_id/service_id 0");
    } else {
      comment->host_id = host_id;
      if (comment->host_id == 0)
        throw exceptions::msg_fmt("comment created from a host with host_id 0");
    }
    comment->poller_id = cbm->poller_id();
    comment->internal_id = comment_id;
    comment->persistent = persistent;
    comment->source = source;

    // Send event.
    cbm->write(comment);
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(
        neb_logger,
        "callbacks: error occurred while generating comment event: {}",
        e.what());
  }
  // Avoid exception propagation in C code.
  catch (...) {
  }
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
static void forward_pb_comment(
    int type,
    com::centreon::engine::comment::type comment_type,
    com::centreon::engine::comment::e_type entry_type,
    uint64_t host_id,
    uint64_t service_id,
    time_t entry_time,
    const char* author_name,
    const char* comment_text,
    bool persistent,
    com::centreon::engine::comment::src source,
    bool expires,
    time_t expire_time,
    uint64_t comment_id) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger, "callbacks: generating pb comment event");

  auto h{std::make_shared<neb::pb_comment>()};
  com::centreon::broker::Comment& comment = h.get()->mut_obj();

  // Fill output var.
  if (author_name)
    comment.set_author(common::check_string_utf8(author_name));
  if (comment_text)
    comment.set_data(common::check_string_utf8(comment_text));
  comment.set_type(comment_type == com::centreon::engine::comment::type::host
                       ? com::centreon::broker::Comment_Type_HOST
                       : com::centreon::broker::Comment_Type_SERVICE);
  if (NEBTYPE_COMMENT_DELETE == type) {
    comment.set_deletion_time(time(nullptr));
    neb_logger->debug("callbacks: comment with deletion time {}",
                      comment.deletion_time());
  }
  comment.set_entry_time(entry_time);
  switch (entry_type) {
    case com::centreon::engine::comment::e_type::user:
      comment.set_entry_type(com::centreon::broker::Comment_EntryType_USER);
      neb_logger->debug("callbacks: comment from a user");
      break;
    case com::centreon::engine::comment::e_type::downtime:
      comment.set_entry_type(com::centreon::broker::Comment_EntryType_DOWNTIME);
      neb_logger->debug("callbacks: comment about downtime");
      break;
    case com::centreon::engine::comment::e_type::flapping:
      comment.set_entry_type(com::centreon::broker::Comment_EntryType_FLAPPING);
      neb_logger->debug("callbacks: comment about flapping");
      break;
    case com::centreon::engine::comment::e_type::acknowledgment:
      comment.set_entry_type(
          com::centreon::broker::Comment_EntryType_ACKNOWLEDGMENT);
      neb_logger->debug("callbacks: comment about acknowledgement");
      break;
    default:
      break;
  }
  comment.set_expire_time(expire_time);
  comment.set_expires(expires);
  if (service_id) {
    if (!host_id) {
      SPDLOG_LOGGER_ERROR(
          neb_logger,
          "comment created from a service with host_id/service_id 0");
      return;
    }
    comment.set_host_id(host_id);
    comment.set_service_id(service_id);
  } else {
    if (host_id == 0) {
      SPDLOG_LOGGER_ERROR(neb_logger,
                          "comment created from a host with host_id 0");
      return;
    }
    comment.set_host_id(host_id);
    comment.set_service_id(0);
  }
  comment.set_instance_id(cbm->poller_id());
  comment.set_internal_id(comment_id);
  comment.set_persistent(persistent);
  comment.set_source(source == com::centreon::engine::comment::src::internal
                         ? com::centreon::broker::Comment_Src_INTERNAL
                         : com::centreon::broker::Comment_Src_EXTERNAL);

  // Send event.
  cbm->write(h);
}

/**
 *  Send comment data to broker.
 *
 *  @param[in] type            Type.
 *  @param[in] comment_type    Comment type.
 *  @param[in] entry_type      Entry type.
 *  @param[in] host_id         Host id.
 *  @param[in] svc_id          Service id.
 *  @param[in] entry_time      Entry time.
 *  @param[in] author_name     Author name.
 *  @param[in] comment_data    Comment data.
 *  @param[in] persistent      Is this comment persistent.
 *  @param[in] source          Comment source.
 *  @param[in] expires         Does this comment expire ?
 *  @param[in] expire_time     Comment expiration time.
 *  @param[in] comment_id      Comment ID.
 */
void broker_comment_data(int type,
                         com::centreon::engine::comment::type comment_type,
                         com::centreon::engine::comment::e_type entry_type,
                         uint64_t host_id,
                         uint64_t service_id,
                         time_t entry_time,
                         char const* author_name,
                         char const* comment_data,
                         int persistent,
                         com::centreon::engine::comment::src source,
                         int expires,
                         time_t expire_time,
                         unsigned long comment_id) {
  // Config check.
  if (!(pb_config.event_broker_options() & BROKER_COMMENT_DATA))
    return;

  // Make callbacks.
  if (cbm->use_protobuf())
    forward_pb_comment(type, comment_type, entry_type, host_id, service_id,
                       entry_time, author_name, comment_data, persistent,
                       source, expires, expire_time, comment_id);
  else
    forward_comment(type, comment_type, entry_type, host_id, service_id,
                    entry_time, author_name, comment_data, persistent, source,
                    expires, expire_time, comment_id);
}

/**
 * @brief process custom variable data.
 *
 * @param cvar custom variable data.
 */
template <typename R>
static void forward_custom_variable(int type,
                                    R* object_ptr,
                                    const std::string_view& var_name,
                                    const std::string_view& var_value,
                                    const struct timeval* timestamp) {
  // Log message.
  SPDLOG_LOGGER_DEBUG(neb_logger,
                      "callbacks: generating custom variable event");

  // Input variable.
  if (!var_name.empty() && !var_value.empty()) {
    // Host custom variable.
    if constexpr (std::is_same_v<R, engine::host>) {
      switch (type) {
        case NEBTYPE_HOSTCUSTOMVARIABLE_ADD: {
          if (object_ptr && !object_ptr->name().empty()) {
            // Fill custom variable event.
            uint64_t host_id = engine::get_host_id(object_ptr->name());
            if (host_id != 0) {
              auto new_cvar = std::make_shared<neb::custom_variable>();
              new_cvar->enabled = true;
              new_cvar->host_id = host_id;
              new_cvar->modified = false;
              new_cvar->name = common::check_string_utf8(var_name);
              new_cvar->var_type = 0;
              if (timestamp)
                new_cvar->update_time = timestamp->tv_sec;
              else {
                struct timeval now;
                gettimeofday(&now, NULL);
                new_cvar->update_time = now.tv_sec;
              }
              new_cvar->value = common::check_string_utf8(var_value);
              new_cvar->default_value = common::check_string_utf8(var_value);

              // Send custom variable event.
              SPDLOG_LOGGER_DEBUG(
                  neb_logger, "callbacks: new custom variable '{}' on host {}",
                  new_cvar->name, new_cvar->host_id);
              cbm->write(new_cvar);
            }
          }
        } break;
        case NEBTYPE_HOSTCUSTOMVARIABLE_DELETE: {
          if (object_ptr && !object_ptr->name().empty()) {
            uint32_t host_id = engine::get_host_id(object_ptr->name());
            if (host_id != 0) {
              auto old_cvar{std::make_shared<neb::custom_variable>()};
              old_cvar->enabled = false;
              old_cvar->host_id = host_id;
              old_cvar->name = common::check_string_utf8(var_name);
              old_cvar->var_type = 0;
              if (timestamp)
                old_cvar->update_time = timestamp->tv_sec;
              else {
                struct timeval now;
                gettimeofday(&now, NULL);
                old_cvar->update_time = now.tv_sec;
              }

              // Send custom variable event.
              SPDLOG_LOGGER_DEBUG(
                  neb_logger,
                  "callbacks: deleted custom variable '{}' on host {}",
                  old_cvar->name, old_cvar->host_id);
              cbm->write(old_cvar);
            }
          }
        } break;
      }
    } else if constexpr (std::is_same_v<R, engine::service>) {
      // Service custom variable.
      switch (type) {
        case NEBTYPE_SERVICECUSTOMVARIABLE_ADD: {
          if (object_ptr && !object_ptr->description().empty() &&
              !object_ptr->get_hostname().empty()) {
            // Fill custom variable event.
            std::pair<uint32_t, uint32_t> p;
            p = engine::get_host_and_service_id(object_ptr->get_hostname(),
                                                object_ptr->description());
            if (p.first && p.second) {
              auto new_cvar{std::make_shared<neb::custom_variable>()};
              new_cvar->enabled = true;
              new_cvar->host_id = p.first;
              new_cvar->modified = false;
              new_cvar->name = common::check_string_utf8(var_name);
              new_cvar->service_id = p.second;
              new_cvar->var_type = 1;
              if (timestamp)
                new_cvar->update_time = timestamp->tv_sec;
              else {
                struct timeval now;
                gettimeofday(&now, NULL);
                new_cvar->update_time = now.tv_sec;
              }
              new_cvar->value = common::check_string_utf8(var_value);
              new_cvar->default_value = common::check_string_utf8(var_value);

              // Send custom variable event.
              SPDLOG_LOGGER_DEBUG(
                  neb_logger,
                  "callbacks: new custom variable '{}' on service ({}, {})",
                  new_cvar->name, new_cvar->host_id, new_cvar->service_id);
              cbm->write(new_cvar);
            }
          }
        } break;
        case NEBTYPE_SERVICECUSTOMVARIABLE_DELETE: {
          if (object_ptr && !object_ptr->description().empty() &&
              !object_ptr->get_hostname().empty()) {
            const std::pair<uint64_t, uint64_t> p{
                engine::get_host_and_service_id(object_ptr->get_hostname(),
                                                object_ptr->description())};
            if (p.first && p.second) {
              auto old_cvar{std::make_shared<neb::custom_variable>()};
              old_cvar->enabled = false;
              old_cvar->host_id = p.first;
              old_cvar->modified = true;
              old_cvar->name = common::check_string_utf8(var_name);
              old_cvar->service_id = p.second;
              old_cvar->var_type = 1;
              if (timestamp)
                old_cvar->update_time = timestamp->tv_sec;
              else {
                struct timeval now;
                gettimeofday(&now, NULL);
                old_cvar->update_time = now.tv_sec;
              }

              // Send custom variable event.
              SPDLOG_LOGGER_DEBUG(
                  neb_logger,
                  "callbacks: deleted custom variable '{}' on service ({},{})",
                  old_cvar->name, old_cvar->host_id, old_cvar->service_id);
              cbm->write(old_cvar);
            }
          }
        }
      }
    }
  }
}

/**
 * @brief process custom variable data.
 *
 * @param cvar custom variable data.
 */
template <typename R>
static void forward_pb_custom_variable(int type,
                                       R* object_ptr,
                                       const std::string_view& var_name,
                                       const std::string_view& var_value,
                                       const struct timeval* timestamp) {
  SPDLOG_LOGGER_DEBUG(neb_logger,
                      "callbacks: generating custom variable event {} value:{}",
                      var_name, var_value);

  auto cv = std::make_shared<neb::pb_custom_variable>();
  neb::pb_custom_variable::pb_type& obj = cv->mut_obj();
  bool ok_to_send = false;
  if (!var_name.empty() && !var_value.empty()) {
    // Host custom variable.
    if constexpr (std::is_same_v<R, engine::host>) {
      if (NEBTYPE_HOSTCUSTOMVARIABLE_ADD == type ||
          NEBTYPE_HOSTCUSTOMVARIABLE_DELETE == type) {
        if (object_ptr && !object_ptr->name().empty()) {
          uint64_t host_id = engine::get_host_id(object_ptr->name());
          if (host_id != 0) {
            std::string name(common::check_string_utf8(var_name));
            bool add = NEBTYPE_HOSTCUSTOMVARIABLE_ADD == type;
            obj.set_enabled(add);
            obj.set_host_id(host_id);
            obj.set_modified(!add);
            obj.set_name(name);
            obj.set_type(com::centreon::broker::CustomVariable_VarType_HOST);
            if (timestamp)
              obj.set_update_time(timestamp->tv_sec);
            else {
              struct timeval now;
              gettimeofday(&now, NULL);
              obj.set_update_time(now.tv_sec);
            }
            if (add) {
              std::string value(common::check_string_utf8(var_value));
              obj.set_value(value);
              obj.set_default_value(value);
              SPDLOG_LOGGER_DEBUG(neb_logger,
                                  "callbacks: new custom variable '{}' with "
                                  "value '{}' on host {}",
                                  name, value, host_id);
            } else {
              SPDLOG_LOGGER_DEBUG(
                  neb_logger,
                  "callbacks: deleted custom variable '{}' on host {}", name,
                  host_id);
            }
            ok_to_send = true;
          }
        }
      }
    } else if constexpr (std::is_same_v<R, engine::service>) {
      // Service custom variable.
      if (NEBTYPE_SERVICECUSTOMVARIABLE_ADD == type ||
          NEBTYPE_SERVICECUSTOMVARIABLE_DELETE == type) {
        if (object_ptr && !object_ptr->description().empty() &&
            !object_ptr->get_hostname().empty()) {
          // Fill custom variable event.
          std::pair<uint64_t, uint64_t> p;
          p = engine::get_host_and_service_id(object_ptr->get_hostname(),
                                              object_ptr->description());
          if (p.first && p.second) {
            std::string name(common::check_string_utf8(var_name));
            bool add = NEBTYPE_SERVICECUSTOMVARIABLE_ADD == type;
            obj.set_enabled(add);
            obj.set_host_id(p.first);
            obj.set_modified(!add);
            obj.set_service_id(p.second);
            obj.set_name(name);
            obj.set_type(com::centreon::broker::CustomVariable_VarType_SERVICE);
            if (timestamp)
              obj.set_update_time(timestamp->tv_sec);
            else {
              struct timeval now;
              gettimeofday(&now, NULL);
              obj.set_update_time(now.tv_sec);
            }
            if (add) {
              std::string value(common::check_string_utf8(var_value));
              obj.set_value(value);
              obj.set_default_value(value);
              SPDLOG_LOGGER_DEBUG(
                  neb_logger,
                  "callbacks: new custom variable '{}' on service ({}, {})",
                  name, p.first, p.second);

            } else {
              SPDLOG_LOGGER_DEBUG(
                  neb_logger,
                  "callbacks: deleted custom variable '{}' on service ({},{})",
                  name, p.first, p.second);
            }
            ok_to_send = true;
          }
        }
      }
    }
  }
  // Send event.
  if (ok_to_send) {
    cbm->write(cv);
  }
}

/**
 *  Sends host custom variables updates to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] data      Host or service.
 *  @param[in] varname   Variable name.
 *  @param[in] varvalue  Variable value.
 *  @param[in] timestamp Timestamp.
 */
template <typename R>
void broker_custom_variable(int type,
                            R* resource,
                            const std::string_view& varname,
                            const std::string_view& varvalue,
                            const struct timeval* timestamp) {
  // Config check.
  if (!(pb_config.event_broker_options() & BROKER_CUSTOMVARIABLE_DATA))
    return;

  // Make callback.
  if (cbm->use_protobuf())
    forward_pb_custom_variable(type, resource, varname, varvalue, timestamp);
  else
    forward_custom_variable(type, resource, varname, varvalue, timestamp);
}

template void broker_custom_variable(int type,
                                     engine::host* resource,
                                     const std::string_view& varname,
                                     const std::string_view& varvalue,
                                     const struct timeval* timestamp);
template void broker_custom_variable(int type,
                                     engine::service* resource,
                                     const std::string_view& varname,
                                     const std::string_view& varvalue,
                                     const struct timeval* timestamp);

/** This implementation is ready but in fact never used */
template void broker_custom_variable(int type,
                                     engine::contact* resource,
                                     const std::string_view& varname,
                                     const std::string_view& varvalue,
                                     const struct timeval* timestamp);

/**
 *  Send downtime data to broker.
 *
 *  @param[in] type            Type.
 *  @param[in] attr            Attributes.
 *  @param[in] downtime_type   Downtime type.
 *  @param[in] host_name       Host name.
 *  @param[in] svc_description Service description.
 *  @param[in] entry_time      Downtime entry time.
 *  @param[in] author_name     Author name.
 *  @param[in] comment_data    Comment.
 *  @param[in] start_time      Downtime start time.
 *  @param[in] end_time        Downtime end time.
 *  @param[in] fixed           Is this a fixed or flexible downtime ?
 *  @param[in] triggered_by    ID of downtime which triggered this downtime.
 *  @param[in] duration        Duration.
 *  @param[in] downtime_id     Downtime ID.
 *  @param[in] timestamp       Timestamp.
 */
void broker_downtime_data(int type,
                          int attr,
                          int downtime_type,
                          uint64_t host_id,
                          uint64_t service_id,
                          time_t entry_time,
                          char const* author_name,
                          char const* comment_data,
                          time_t start_time,
                          time_t end_time,
                          bool fixed,
                          unsigned long triggered_by,
                          unsigned long duration,
                          unsigned long downtime_id,
                          struct timeval const* timestamp) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_DOWNTIME_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_DOWNTIME_DATA))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_downtime_data ds;
  ds.type = type;
  ds.attr = attr;
  ds.timestamp = get_broker_timestamp(timestamp);
  ds.downtime_type = downtime_type;
  ds.host_id = host_id;
  ds.service_id = service_id;
  ds.entry_time = entry_time;
  ds.author_name = author_name;
  ds.comment_data = comment_data;
  ds.start_time = start_time;
  ds.end_time = end_time;
  ds.fixed = fixed;
  ds.duration = duration;
  ds.triggered_by = triggered_by;
  ds.downtime_id = downtime_id;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_DOWNTIME_DATA, &ds);
}

/**
 *  Sends external commands to broker.
 *
 *  @param[in] type           Type.
 *  @param[in] command_type   Command type.
 *  @param[in] command_args   Command args.
 *  @param[in] timestamp      Timestamp.
 */
void broker_external_command(int type,
                             int command_type,
                             char* command_args,
                             struct timeval const* timestamp) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_EXTERNALCOMMAND_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_EXTERNALCOMMAND_DATA))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_external_command_data ds;
  ds.type = type;
  ds.timestamp = get_broker_timestamp(timestamp);
  ds.command_type = command_type;
  ds.command_args = command_args;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_EXTERNAL_COMMAND_DATA, &ds);
}

/**
 *  Send group update to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] data      Host group or service group.

 */
void broker_group(int type, void* data) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_GROUP_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_GROUP_DATA))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_group_data ds;
  ds.type = type;
  ds.object_ptr = data;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_GROUP_DATA, &ds);
}

/**
 *  Send group membership to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] object    Member (host or service).
 *  @param[in] group     Group (host or service).
 */
void broker_group_member(int type, void* object, void* group) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_GROUP_MEMBER_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_GROUP_MEMBER_DATA))
    return;
#endif

  // Fill struct will relevant data.
  nebstruct_group_member_data ds;
  ds.type = type;
  ds.object_ptr = object;
  ds.group_ptr = group;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_GROUP_MEMBER_DATA, &ds);
}

/**
 *  Send host check data to broker.
 *
 *  @param[in] type          Type.
 *  @param[in] hst           Host.
 *  @param[in] check_type    Check type.
 *  @param[in] cmdline       Command line.
 *  @param[in] output        Output.
 *
 *  @return Return value can override host check.
 */
int broker_host_check(int type,
                      host* hst,
                      int check_type,
                      char const* cmdline,
                      char* output) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_HOST_CHECKS))
    return OK;
#else
  if (!(pb_config.event_broker_options() & BROKER_HOST_CHECKS))
    return OK;
#endif
  if (!hst)
    return ERROR;

  // Fill struct with relevant data.
  nebstruct_host_check_data ds;
  ds.type = type;
  ds.host_name = const_cast<char*>(hst->name().c_str());
  ds.object_ptr = hst;
  ds.check_type = check_type;
  ds.command_line = cmdline;
  ds.output = output;

  // Make callbacks.
  int return_code;
  return_code = neb_make_callbacks(NEBCALLBACK_HOST_CHECK_DATA, &ds);

  // Free data.
  return return_code;
}

/**
 *  Sends host status updates to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] hst       Host.
 *  @param[in] attributes Attributes from status_attribute enumeration.
 */
void broker_host_status(int type, host* hst, uint32_t attributes) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_STATUS_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_STATUS_DATA))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_host_status_data ds;
  ds.type = type;
  ds.object_ptr = hst;
  ds.attributes = attributes;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_HOST_STATUS_DATA, &ds);
}

/**
 *  Send log data to broker.
 *
 *  @param[in] data       Log entry.
 *  @param[in] entry_time Entry time.
 */
void broker_log_data(char* data, time_t entry_time) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_LOGGED_DATA) ||
      !config->log_legacy_enabled())
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_LOGGED_DATA) ||
      !pb_config.log_legacy_enabled())
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_log_data ds;
  ds.entry_time = entry_time;
  ds.data = data;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_LOG_DATA, &ds);
}

/**
 *  Send notification data to broker.
 *
 *  @param[in] type              Type.
 *  @param[in] flags             Flags.
 *  @param[in] attr              Attributes.
 *  @param[in] notification_type Notification type.
 *  @param[in] reason_type       Reason type.
 *  @param[in] start_time        Start time.
 *  @param[in] end_time          End time.
 *  @param[in] data              Data.
 *  @param[in] ack_author        Acknowledgement author.
 *  @param[in] ack_data          Acknowledgement data.
 *  @param[in] escalated         Is notification escalated ?
 *  @param[in] contacts_notified Are contacts notified ?
 *  @param[in] timestamp         Timestamp.
 *
 *  @return Return value can override notification.
 */
int broker_notification_data(int type [[maybe_unused]],
                             int flags [[maybe_unused]],
                             int attr [[maybe_unused]],
                             unsigned int notification_type [[maybe_unused]],
                             int reason_type [[maybe_unused]],
                             struct timeval start_time [[maybe_unused]],
                             struct timeval end_time [[maybe_unused]],
                             void* data [[maybe_unused]],
                             char const* ack_author [[maybe_unused]],
                             char const* ack_data [[maybe_unused]],
                             int escalated [[maybe_unused]],
                             int contacts_notified [[maybe_unused]],
                             struct timeval const* timestamp [[maybe_unused]]) {
  return 0;
}

/**
 *  Sends program data (starts, restarts, stops, etc.) to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] flags     Flags.
 */
void broker_program_state(int type, int flags) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_PROGRAM_STATE))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_PROGRAM_STATE))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_process_data ds;
  ds.type = type;
  ds.flags = flags;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_PROCESS_DATA, &ds);
}

/**
 *  Sends program status updates to broker.
 */
void broker_program_status() {
#ifdef LEGACY_CONF
  // Config check.
  if (!(config->event_broker_options() & BROKER_STATUS_DATA))
    return;

  // Fill struct with relevant data.
  nebstruct_program_status_data ds;
  ds.last_command_check = last_command_check;
  ds.notifications_enabled = config->enable_notifications();
  ds.active_service_checks_enabled = config->execute_service_checks();
  ds.passive_service_checks_enabled = config->accept_passive_service_checks();
  ds.active_host_checks_enabled = config->execute_host_checks();
  ds.passive_host_checks_enabled = config->accept_passive_host_checks();
  ds.event_handlers_enabled = config->enable_event_handlers();
  ds.flap_detection_enabled = config->enable_flap_detection();
  ds.obsess_over_hosts = config->obsess_over_hosts();
  ds.obsess_over_services = config->obsess_over_services();
  ds.global_host_event_handler = config->global_host_event_handler();
  ds.global_service_event_handler = config->global_service_event_handler();

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_PROGRAM_STATUS_DATA, &ds);
#else
  // Config check.
  if (!(pb_config.event_broker_options() & BROKER_STATUS_DATA))
    return;

  // Fill struct with relevant data.
  nebstruct_program_status_data ds;
  ds.last_command_check = last_command_check;
  ds.notifications_enabled = pb_config.enable_notifications();
  ds.active_service_checks_enabled = pb_config.execute_service_checks();
  ds.passive_service_checks_enabled = pb_config.accept_passive_service_checks();
  ds.active_host_checks_enabled = pb_config.execute_host_checks();
  ds.passive_host_checks_enabled = pb_config.accept_passive_host_checks();
  ds.event_handlers_enabled = pb_config.enable_event_handlers();
  ds.flap_detection_enabled = pb_config.enable_flap_detection();
  ds.obsess_over_hosts = pb_config.obsess_over_hosts();
  ds.obsess_over_services = pb_config.obsess_over_services();
  ds.global_host_event_handler = pb_config.global_host_event_handler();
  ds.global_service_event_handler = pb_config.global_service_event_handler();

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_PROGRAM_STATUS_DATA, &ds);
#endif
}

/**
 *  Send relationship data to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] hst       Host.
 *  @param[in] svc       Service (might be null).
 *  @param[in] dep_hst   Dependant host object.
 *  @param[in] dep_svc   Dependant service object (might be null).
 */
void broker_relation_data(int type,
                          host* hst,
                          com::centreon::engine::service* svc,
                          host* dep_hst,
                          com::centreon::engine::service* dep_svc) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_RELATION_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_RELATION_DATA))
    return;
#endif
  if (!hst || !dep_hst)
    return;

  // Fill struct with relevant data.
  nebstruct_relation_data ds;
  ds.type = type;
  ds.hst = hst;
  ds.svc = svc;
  ds.dep_hst = dep_hst;
  ds.dep_svc = dep_svc;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_RELATION_DATA, &ds);
}

/**
 *  Brokers retention data.
 *
 *  @param[in] type      Type.
 *  @param[in] flags     Flags.
 *  @param[in] attr      Attributes.
 *  @param[in] timestamp Timestamp.
 */
void broker_retention_data(int type __attribute__((unused)),
                           int flags __attribute__((unused)),
                           int attr __attribute__((unused)),
                           struct timeval const* timestamp
                           __attribute__((unused))) {}

/**
 *  Send service check data to broker.
 *
 *  @param[in] type          Type.
 *  @param[in] svc           Target service.
 *  @param[in] check_type    Check type.
 *  @param[in] cmdline       Check command line.
 *
 *  @return Return value can override service check.
 */
int broker_service_check(int type,
                         com::centreon::engine::service* svc,
                         int check_type,
                         const char* cmdline) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_SERVICE_CHECKS))
    return OK;
#else
  if (!(pb_config.event_broker_options() & BROKER_SERVICE_CHECKS))
    return OK;
#endif
  if (!svc)
    return ERROR;

  // Fill struct with relevant data.
  nebstruct_service_check_data ds;
  ds.type = type;
  ds.host_id = svc->host_id();
  ds.service_id = svc->service_id();
  ds.object_ptr = svc;
  ds.check_type = check_type;
  ds.command_line = cmdline;
  ds.output = const_cast<char*>(svc->get_plugin_output().c_str());

  // Make callbacks.
  int return_code;
  return_code = neb_make_callbacks(NEBCALLBACK_SERVICE_CHECK_DATA, &ds);

  return return_code;
}

/**
 *  Sends service status updates to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] svc       Target service.
 *  @param[in] attributes Attributes from status_attribute enumeration.
 */
void broker_service_status(int type,
                           com::centreon::engine::service* svc,
                           uint32_t attributes) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_STATUS_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_STATUS_DATA))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_service_status_data ds;
  ds.type = type;
  ds.object_ptr = svc;
  ds.attributes = attributes;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_SERVICE_STATUS_DATA, &ds);
}

/**
 *  Send state change data to broker.
 *
 *  @param[in] type             Type.
 *  @param[in] flags            Flags.
 *  @param[in] attr             Attributes.
 *  @param[in] statechange_type State change type.
 *  @param[in] data             Data.
 *  @param[in] state            State.
 *  @param[in] state_type       State type.
 *  @param[in] current_attempt  Current attempt.
 *  @param[in] max_attempts     Max attempts.
 *  @param[in] timestamp        Timestamp.
 */
void broker_statechange_data(int type __attribute__((unused)),
                             int flags __attribute__((unused)),
                             int attr __attribute__((unused)),
                             int statechange_type __attribute__((unused)),
                             void* data __attribute__((unused)),
                             int state __attribute__((unused)),
                             int state_type __attribute__((unused)),
                             int current_attempt __attribute__((unused)),
                             int max_attempts __attribute__((unused)),
                             struct timeval const* timestamp
                             __attribute__((unused))) {}

/**
 *  Send system command data to broker.
 *
 *  @param[in] type          Type.
 *  @param[in] flags         Flags.
 *  @param[in] attr          Attributes.
 *  @param[in] start_time    Start time.
 *  @param[in] end_time      End time.
 *  @param[in] exectime      Execution time.
 *  @param[in] timeout       Timeout.
 *  @param[in] early_timeout Early timeout.
 *  @param[in] retcode       Return code.
 *  @param[in] cmd           Command.
 *  @param[in] output        Output.
 *  @param[in] timestamp     Timestamp.
 */
void broker_system_command(int type __attribute__((unused)),
                           int flags __attribute__((unused)),
                           int attr __attribute__((unused)),
                           struct timeval start_time __attribute__((unused)),
                           struct timeval end_time __attribute__((unused)),
                           double exectime __attribute__((unused)),
                           int timeout __attribute__((unused)),
                           int early_timeout __attribute__((unused)),
                           int retcode __attribute__((unused)),
                           const char* cmd __attribute__((unused)),
                           const char* output __attribute__((unused)),
                           struct timeval const* timestamp
                           __attribute__((unused))) {}

/**
 *  Send timed event data to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] flags     Flags.
 *  @param[in] attr      Attributes.
 *  @param[in] event     Target event.
 *  @param[in] timestamp Timestamp.
 */
void broker_timed_event(int type __attribute__((unused)),
                        int flags __attribute__((unused)),
                        int attr __attribute__((unused)),
                        com::centreon::engine::timed_event* event
                        __attribute__((unused)),
                        struct timeval const* timestamp
                        __attribute__((unused))) {}

/**
 *  Gets timestamp for use by broker.
 *
 *  @param[in] timestamp Timestamp.
 */
struct timeval get_broker_timestamp(struct timeval const* timestamp) {
  struct timeval tv;
  if (!timestamp)
    gettimeofday(&tv, NULL);
  else
    tv = *timestamp;
  return (tv);
}

/**
 *  Sends bench message over network.
 *
 *  @param[in] id      id.
 *  @param[in] time_create       message creation
 */
void broker_bench(unsigned id,
                  const std::chrono::system_clock::time_point& mess_create) {
  // Fill struct with relevant data.
  nebstruct_bench_data ds = {id, mess_create};
  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_BENCH_DATA, &ds);
}

/**
 * @brief send agent usage statistics to broker
 *
 * @param stats
 */
void broker_agent_stats(nebstruct_agent_stats_data& stats) {
  // Fill struct with relevant data.
  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_AGENT_STATS, &stats);
}
