/**
 * Copyright 2002-2010      Ethan Galstad
 * Copyright 2010           Nagios Core Development Team
 * Copyright 2011-2013,2020-2024 Centreon
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

#include "com/centreon/engine/broker.hh"
#include <absl/strings/str_split.h>
#include <unistd.h>
#include "com/centreon/engine/flapping.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/nebstructs.hh"
#include "com/centreon/engine/sehandlers.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon::engine;

extern "C" {

/**
 *  Send acknowledgement data to broker.
 *
 *  @param[in] type                 Type.
 *  @param[in] acknowledgement_type Type (host or service).
 *  @param[in] data                 Data.
 *  @param[in] ack_author           Author.
 *  @param[in] ack_data             Acknowledgement text.
 *  @param[in] subtype              Subtype.
 *  @param[in] notify_contacts      Should we notify contacts.
 *  @param[in] persistent_comment   Persistent comment
 */
void broker_acknowledgement_data(
    int type,
    acknowledgement_resource_type acknowledgement_type,
    void* data,
    const char* ack_author,
    const char* ack_data,
    int subtype,
    int notify_contacts,
    int persistent_comment) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_ACKNOWLEDGEMENT_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_ACKNOWLEDGEMENT_DATA))
    return;
#endif

  // Fill struct with relevant data.
  host* temp_host(NULL);
  com::centreon::engine::service* temp_service(NULL);
  nebstruct_acknowledgement_data ds;
  ds.type = type;
  ds.acknowledgement_type = acknowledgement_type;
  if (acknowledgement_type == acknowledgement_resource_type::SERVICE) {
    temp_service = (com::centreon::engine::service*)data;
    ds.host_id = temp_service->host_id();
    ds.service_id = temp_service->service_id();
    ds.state = temp_service->get_current_state();
  } else {
    temp_host = (host*)data;
    ds.host_id = temp_host->host_id();
    ds.service_id = 0;
    ds.state = temp_host->get_current_state();
  }
  ds.author_name = ack_author;
  ds.comment_data = ack_data;
  ds.is_sticky = (subtype == AckType::STICKY);
  ds.notify_contacts = notify_contacts;
  ds.persistent_comment = persistent_comment;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_ACKNOWLEDGEMENT_DATA, &ds);
}

/**
 *  Sends adaptive contact updates to broker.
 *
 *  @param[in] type         Type.
 *  @param[in] flags        Flags.
 *  @param[in] attr         Attributes.
 *  @param[in] cntct        Target contact.
 *  @param[in] command_type Command type.
 *  @param[in] modattr      Global contact modified attributes.
 *  @param[in] modattrs     Target contact modified attributes.
 *  @param[in] modhattr     Global contact modified host attributes.
 *  @param[in] modhattrs    Target contact modified attributes.
 *  @param[in] modsattr     Global contact modified service attributes.
 *  @param[in] modsattrs    Target contact modified service attributes.
 *  @param[in] timestamp    Timestamp.
 */
void broker_adaptive_contact_data(
    int type __attribute__((unused)),
    int flags __attribute__((unused)),
    int attr __attribute__((unused)),
    contact* cntct __attribute__((unused)),
    int command_type __attribute__((unused)),
    unsigned long modattr __attribute__((unused)),
    unsigned long modattrs __attribute__((unused)),
    unsigned long modhattr __attribute__((unused)),
    unsigned long modhattrs __attribute__((unused)),
    unsigned long modsattr __attribute__((unused)),
    unsigned long modsattrs __attribute__((unused)),
    struct timeval const* timestamp __attribute__((unused))) {}

/**
 * @brief Send adaptive severity updates to broker.
 *
 * @param type      Type.
 * @param data      Target severity.
 */
void broker_adaptive_severity_data(int type, void* data) {
  /* Config check. */
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;
#endif

  /* Fill struct with relevant data. */
  nebstruct_adaptive_severity_data ds;
  ds.type = type;
  ds.object_ptr = data;

  /* Make callbacks. */
  neb_make_callbacks(NEBCALLBACK_ADAPTIVE_SEVERITY_DATA, &ds);
}

/**
 * @brief Send adaptive tag updates to broker.
 *
 * @param type      Type.
 * @param data      Target tag.
 */
void broker_adaptive_tag_data(int type, void* data) {
  /* Config check. */
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;
#endif

  /* Fill struct with relevant data. */
  nebstruct_adaptive_tag_data ds;
  ds.type = type;
  ds.object_ptr = data;

  /* Make callbacks. */
  neb_make_callbacks(NEBCALLBACK_ADAPTIVE_TAG_DATA, &ds);
}

/**
 *  Send adaptive dependency updates to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] data      Target dependency.
 */
void broker_adaptive_dependency_data(int type, void* data) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_adaptive_dependency_data ds;
  ds.type = type;
  ds.object_ptr = data;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_ADAPTIVE_DEPENDENCY_DATA, &ds);
}

/**
 *  Sends adaptative escalation updates to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] flags     Flags.
 *  @param[in] attr      Attributes.
 *  @param[in] data      Target escalation.
 *  @param[in] timestamp Timestamp.
 */
void broker_adaptive_escalation_data(int type __attribute__((unused)),
                                     int flags __attribute__((unused)),
                                     int attr __attribute__((unused)),
                                     void* data __attribute__((unused)),
                                     struct timeval const* timestamp
                                     __attribute__((unused))) {}

/**
 *  Sends adaptive host updates to broker.
 *
 *  @param[in] type         Type.
 *  @param[in] flags        Flags.
 *  @param[in] attr         Attributes.
 *  @param[in] hst          Target host.
 *  @param[in] modattr      Global host modified attributes.
 */
void broker_adaptive_host_data(int type,
                               int flags,
                               int attr,
                               host* hst,
                               unsigned long modattr) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_adaptive_host_data ds;
  ds.type = type;
  ds.flags = flags;
  ds.attr = attr;
  ds.modified_attribute = modattr;
  ds.object_ptr = hst;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_ADAPTIVE_HOST_DATA, &ds);
}

/**
 *  Sends adaptive programs updates to broker.
 *
 *  @param[in] type         Type.
 *  @param[in] flags        Flags.
 *  @param[in] attr         Attributes.
 *  @param[in] command_type Command type.
 *  @param[in] modhattr     Global host modified attributes.
 *  @param[in] modhattrs    Specific host modified attributes.
 *  @param[in] modsattr     Global service modified attributes.
 *  @param[in] modsattrs    Specific service modified attributes.
 *  @param[in] timestamp    Timestamp.
 */
void broker_adaptive_program_data(
    int type __attribute__((unused)),
    int flags __attribute__((unused)),
    int attr __attribute__((unused)),
    int command_type __attribute__((unused)),
    unsigned long modhattr __attribute__((unused)),
    unsigned long modhattrs __attribute__((unused)),
    unsigned long modsattr __attribute__((unused)),
    unsigned long modsattrs __attribute__((unused)),
    struct timeval const* timestamp __attribute__((unused))) {}

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
                                  int attr,
                                  com::centreon::engine::service* svc,
                                  unsigned long modattr) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_ADAPTIVE_DATA))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_adaptive_service_data ds;
  ds.type = type;
  ds.flags = flags;
  ds.attr = attr;
  ds.modified_attribute = modattr;
  ds.object_ptr = svc;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_ADAPTIVE_SERVICE_DATA, &ds);
}

/**
 *  Sends adaptive timeperiod updates to broker.
 *
 *  @param[in] type         Type.
 *  @param[in] flags        Flags.
 *  @param[in] attr         Attributes.
 *  @param[in] tp           Target timeperiod.
 *  @param[in] command_type Command type.
 *  @param[in] timestamp    Timestamp.
 */
void broker_adaptive_timeperiod_data(int type __attribute__((unused)),
                                     int flags __attribute__((unused)),
                                     int attr __attribute__((unused)),
                                     timeperiod* tp __attribute__((unused)),
                                     int command_type __attribute__((unused)),
                                     struct timeval const* timestamp
                                     __attribute__((unused))) {}

/**
 *  Brokers aggregated status dumps.
 *
 *  @param[in] type      Type.
 *  @param[in] flags     Flags.
 *  @param[in] attr      Attributes
 *  @param[in] timestamp Timestamp.
 */
void broker_aggregated_status_data(int type __attribute__((unused)),
                                   int flags __attribute__((unused)),
                                   int attr __attribute__((unused)),
                                   struct timeval const* timestamp
                                   __attribute__((unused))) {}

/**
 *  Send command data to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] flags     Flags.
 *  @param[in] attr      Attributes.
 *  @param[in] cmd       The command.
 *  @param[in] timestamp Timestamp.
 */
void broker_command_data(int type __attribute__((unused)),
                         int flags __attribute__((unused)),
                         int attr __attribute__((unused)),
                         commands::command* cmd __attribute__((unused)),
                         struct timeval const* timestamp
                         __attribute__((unused))) {}

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
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_COMMENT_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_COMMENT_DATA))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_comment_data ds;
  ds.type = type;
  ds.comment_type = comment_type;
  ds.entry_type = entry_type;
  ds.host_id = host_id;
  ds.service_id = service_id;
  ds.entry_time = entry_time;
  ds.author_name = author_name;
  ds.comment_data = comment_data;
  ds.persistent = persistent;
  ds.source = source;
  ds.expires = expires;
  ds.expire_time = expire_time;
  ds.comment_id = comment_id;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_COMMENT_DATA, &ds);
}

/**
 *  Send contact notification data to broker.
 *
 *  @param[in] type              Type.
 *  @param[in] flags             Flags.
 *  @param[in] attr              Attributes.
 *  @param[in] notification_type Notification type.
 *  @param[in] reason_type       Notification reason.
 *  @param[in] start_time        Start time.
 *  @param[in] end_time          End time.
 *  @param[in] data              Data.
 *  @param[in] cntct             Target contact.
 *  @param[in] ack_author        Author.
 *  @param[in] ack_data          Data.
 *  @param[in] escalated         Escalated ?
 *  @param[in] timestamp         Timestamp.
 */
int broker_contact_notification_data(int type [[maybe_unused]],
                                     int flags [[maybe_unused]],
                                     int attr [[maybe_unused]],
                                     unsigned int notification_type
                                     [[maybe_unused]],
                                     int reason_type [[maybe_unused]],
                                     struct timeval start_time [[maybe_unused]],
                                     struct timeval end_time [[maybe_unused]],
                                     void* data [[maybe_unused]],
                                     contact* cntct [[maybe_unused]],
                                     char const* ack_author [[maybe_unused]],
                                     char const* ack_data [[maybe_unused]],
                                     int escalated [[maybe_unused]],
                                     struct timeval const* timestamp
                                     [[maybe_unused]]) {
  return 0;
}

/**
 *  Send contact notification data to broker.
 *
 *  @param[in] type              Type.
 *  @param[in] flags             Flags.
 *  @param[in] attr              Attributes.
 *  @param[in] notification_type Notification type.
 *  @param[in] reason_type       Reason type.
 *  @param[in] start_time        Start time.
 *  @param[in] end_time          End time.
 *  @param[in] data              Data.
 *  @param[in] cntct             Target contact.
 *  @param[in] ack_author        Author.
 *  @param[in] ack_data          Data.
 *  @param[in] escalated         Escalated ?
 *  @param[in] timestamp         Timestamp.
 *
 *  @return Return value can override notification.
 */
int broker_contact_notification_method_data(
    int type [[maybe_unused]],
    int flags [[maybe_unused]],
    int attr [[maybe_unused]],
    unsigned int notification_type [[maybe_unused]],
    int reason_type [[maybe_unused]],
    struct timeval start_time [[maybe_unused]],
    struct timeval end_time [[maybe_unused]],
    void* data [[maybe_unused]],
    contact* cntct [[maybe_unused]],
    char const* ack_author [[maybe_unused]],
    char const* ack_data [[maybe_unused]],
    int escalated [[maybe_unused]],
    struct timeval const* timestamp [[maybe_unused]]) {
  return 0;
}

/**
 *  Sends contact status updates to broker.
 *
 *  @param[in] type      Type.
 *  @param[in] cntct     Target contact.
 */
void broker_contact_status(int type, contact* cntct) {
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
  ds.object_ptr = cntct;

  // Make callbacks.
  neb_make_callbacks(NEBCALLBACK_CONTACT_STATUS_DATA, &ds);
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
void broker_custom_variable(int type,
                            void* data,
                            std::string_view&& varname,
                            std::string_view&& varvalue,
                            struct timeval const* timestamp) {
  // Config check.
#ifdef LEGACY_CONF
  if (!(config->event_broker_options() & BROKER_CUSTOMVARIABLE_DATA))
    return;
#else
  if (!(pb_config.event_broker_options() & BROKER_CUSTOMVARIABLE_DATA))
    return;
#endif

  // Fill struct with relevant data.
  nebstruct_custom_variable_data ds{
      .type = type,
      .timestamp = get_broker_timestamp(timestamp),
      .var_name = varname,
      .var_value = varvalue,
      .object_ptr = data,
  };

  // Make callback.
  neb_make_callbacks(NEBCALLBACK_CUSTOM_VARIABLE_DATA, &ds);
}

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
}
