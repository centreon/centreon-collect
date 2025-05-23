/**
 * Copyright 2002-2006 Ethan Galstad
 * Copyright 2011-2013 Merethis
 * Copyright 2018-2024 Centreon
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

#ifndef CCE_BROKER_HH
#define CCE_BROKER_HH

#include "com/centreon/engine/commands/command.hh"
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/events/timed_event.hh"

/* Event broker options. */
#define BROKER_NOTHING 0
#define BROKER_PROGRAM_STATE (1 << 0)
#define BROKER_TIMED_EVENTS (1 << 1)
#define BROKER_SERVICE_CHECKS (1 << 2)
#define BROKER_HOST_CHECKS (1 << 3)
#define BROKER_EVENT_HANDLERS (1 << 4)  // not used since 2022-10-06
#define BROKER_LOGGED_DATA (1 << 5)
#define BROKER_NOTIFICATIONS (1 << 6)
#define BROKER_FLAPPING_DATA (1 << 7) /*not used from 2022-10-06*/
#define BROKER_COMMENT_DATA (1 << 8)
#define BROKER_DOWNTIME_DATA (1 << 9)
#define BROKER_SYSTEM_COMMANDS (1 << 10)
#define BROKER_OCP_DATA_UNUSED (1 << 11) /* Reusable. */
#define BROKER_STATUS_DATA (1 << 12)
#define BROKER_ADAPTIVE_DATA (1 << 13)
#define BROKER_EXTERNALCOMMAND_DATA (1 << 14)
#define BROKER_RETENTION_DATA (1 << 15)
#define BROKER_ACKNOWLEDGEMENT_DATA (1 << 16)
#define BROKER_STATECHANGE_DATA (1 << 17)
#define BROKER_RESERVED18 (1 << 18)
#define BROKER_RESERVED19 (1 << 19)
#define BROKER_CUSTOMVARIABLE_DATA (1 << 20)
#define BROKER_GROUP_DATA (1 << 21)
#define BROKER_GROUP_MEMBER_DATA (1 << 22)
#define BROKER_MODULE_DATA (1 << 23)
#define BROKER_RELATION_DATA (1 << 24)
#define BROKER_COMMAND_DATA (1 << 25)
#define BROKER_EVERYTHING (~0)

/*
** Event types.
*/
#define NEBTYPE_NONE 0

/* Unused ? */
#define NEBTYPE_HELLO 1
#define NEBTYPE_GOODBYE 2
#define NEBTYPE_INFO 3

/* Process. */
#define NEBTYPE_PROCESS_START 100
#define NEBTYPE_PROCESS_DAEMONIZE 101
#define NEBTYPE_PROCESS_RESTART 102
#define NEBTYPE_PROCESS_SHUTDOWN 103
#define NEBTYPE_PROCESS_PRELAUNCH             \
  104 /* Before objects are read or verified. \
       */
#define NEBTYPE_PROCESS_EVENTLOOPSTART 105
#define NEBTYPE_PROCESS_EVENTLOOPEND 106

/* Events. */
#define NEBTYPE_TIMEDEVENT_ADD 200
#define NEBTYPE_TIMEDEVENT_DELETE 201
#define NEBTYPE_TIMEDEVENT_REMOVE NEBTYPE_TIMEDEVENT_DELETE
#define NEBTYPE_TIMEDEVENT_EXECUTE 202
#define NEBTYPE_TIMEDEVENT_DELAY 203 /* NOT IMPLEMENTED. */
#define NEBTYPE_TIMEDEVENT_SKIP 204  /* NOT IMPLEMENTED. */
#define NEBTYPE_TIMEDEVENT_SLEEP 205

/* Logs. */
#define NEBTYPE_LOG_DATA 300
#define NEBTYPE_LOG_ROTATION 301

/* System commands. */
#define NEBTYPE_SYSTEM_COMMAND_START 400
#define NEBTYPE_SYSTEM_COMMAND_END 401

/* Event handlers. */
#define NEBTYPE_EVENTHANDLER_START 500
#define NEBTYPE_EVENTHANDLER_END 501

/* Notifications. */
#define NEBTYPE_NOTIFICATION_START 600
#define NEBTYPE_NOTIFICATION_END 601
#define NEBTYPE_CONTACTNOTIFICATION_START 602
#define NEBTYPE_CONTACTNOTIFICATION_END 603
#define NEBTYPE_CONTACTNOTIFICATIONMETHOD_START 604
#define NEBTYPE_CONTACTNOTIFICATIONMETHOD_END 605

/* Service checks. */
#define NEBTYPE_SERVICECHECK_INITIATE 700
#define NEBTYPE_SERVICECHECK_PROCESSED 701
#define NEBTYPE_SERVICECHECK_RAW_START 702 /* NOT IMPLEMENTED. */
#define NEBTYPE_SERVICECHECK_RAW_END 703   /* NOT IMPLEMENTED. */
#define NEBTYPE_SERVICECHECK_ASYNC_PRECHECK 704

/* Host checks. */
#define NEBTYPE_HOSTCHECK_INITIATE \
  800 /* A check of the route to the host has been initiated. */
#define NEBTYPE_HOSTCHECK_PROCESSED \
  801 /* The processed/final result of a host check. */
#define NEBTYPE_HOSTCHECK_RAW_START 802 /* The start of a "raw" host check. */
#define NEBTYPE_HOSTCHECK_RAW_END 803   /* A finished "raw" host check. */
#define NEBTYPE_HOSTCHECK_ASYNC_PRECHECK 804
#define NEBTYPE_HOSTCHECK_SYNC_PRECHECK 805

/* Comments. */
#define NEBTYPE_COMMENT_ADD 900
#define NEBTYPE_COMMENT_DELETE 901
#define NEBTYPE_COMMENT_LOAD 902

/* Flapping. */
#define NEBTYPE_FLAPPING_START 1000
#define NEBTYPE_FLAPPING_STOP 1001

/* Downtimes. */
#define NEBTYPE_DOWNTIME_ADD 1100
#define NEBTYPE_DOWNTIME_DELETE 1101
#define NEBTYPE_DOWNTIME_LOAD 1102
#define NEBTYPE_DOWNTIME_START 1103
#define NEBTYPE_DOWNTIME_STOP 1104
#define NEBTYPE_DOWNTIME_UPDATE 1105

/* Statuses. */
#define NEBTYPE_PROGRAMSTATUS_UPDATE 1200
#define NEBTYPE_HOSTSTATUS_UPDATE 1201
#define NEBTYPE_SERVICESTATUS_UPDATE 1202
#define NEBTYPE_CONTACTSTATUS_UPDATE 1203

/* Adaptive modifications. */
#define NEBTYPE_ADAPTIVEPROGRAM_UPDATE 1300
#define NEBTYPE_ADAPTIVEHOST_UPDATE 1301
#define NEBTYPE_ADAPTIVESERVICE_UPDATE 1302
#define NEBTYPE_ADAPTIVECONTACT_UPDATE 1303

/* External commands. */
#define NEBTYPE_EXTERNALCOMMAND_START 1400
#define NEBTYPE_EXTERNALCOMMAND_END 1401
#define NEBTYPE_EXTERNALCOMMAND_CHECK 1402

/* Aggregated statuses. */
#define NEBTYPE_AGGREGATEDSTATUS_STARTDUMP 1500
#define NEBTYPE_AGGREGATEDSTATUS_ENDDUMP 1501

/* Retention. */
#define NEBTYPE_RETENTIONDATA_STARTLOAD 1600
#define NEBTYPE_RETENTIONDATA_ENDLOAD 1601
#define NEBTYPE_RETENTIONDATA_STARTSAVE 1602
#define NEBTYPE_RETENTIONDATA_ENDSAVE 1603

/* Acknowledgement. */
#define NEBTYPE_ACKNOWLEDGEMENT_ADD 1700
#define NEBTYPE_ACKNOWLEDGEMENT_DELETE 1701
#define NEBTYPE_ACKNOWLEDGEMENT_REMOVE NEBTYPE_ACKNOWLEDGEMENT_DELETE
#define NEBTYPE_ACKNOWLEDGEMENT_LOAD 1702 /* NOT IMPLEMENTED. */
#define NEBTYPE_ACKNOWLEDGEMENT_UPDATE 1703

/* State change. */
#define NEBTYPE_STATECHANGE_START 1800 /* NOT IMPLEMENTED. */
#define NEBTYPE_STATECHANGE_END 1801

/* Commands. */
#define NEBTYPE_COMMAND_ADD 1900
#define NEBTYPE_COMMAND_DELETE 1901
#define NEBTYPE_COMMAND_UPDATE 1902

/* Contacts. */
#define NEBTYPE_CONTACT_ADD 2000
#define NEBTYPE_CONTACT_DELETE 2001
#define NEBTYPE_CONTACT_UPDATE NEBTYPE_ADAPTIVECONTACT_UPDATE

/* Contact custom variables. */
#define NEBTYPE_CONTACTCUSTOMVARIABLE_ADD 2100
#define NEBTYPE_CONTACTCUSTOMVARIABLE_DELETE 2101
#define NEBTYPE_CONTACTCUSTOMVARIABLE_UPDATE 2102

/* Contact groups. */
#define NEBTYPE_CONTACTGROUP_ADD 2200
#define NEBTYPE_CONTACTGROUP_DELETE 2201
#define NEBTYPE_CONTACTGROUP_UPDATE 2202

/* Contact group members. */
#define NEBTYPE_CONTACTGROUPMEMBER_ADD 2300
#define NEBTYPE_CONTACTGROUPMEMBER_DELETE 2301
#define NEBTYPE_CONTACTGROUPMEMBER_UPDATE 2302

/* Hosts. */
#define NEBTYPE_HOST_ADD 2400
#define NEBTYPE_HOST_DELETE 2401
#define NEBTYPE_HOST_UPDATE NEBTYPE_ADAPTIVEHOST_UPDATE

/* Host custom variables. */
#define NEBTYPE_HOSTCUSTOMVARIABLE_ADD 2500
#define NEBTYPE_HOSTCUSTOMVARIABLE_DELETE 2501
#define NEBTYPE_HOSTCUSTOMVARIABLE_UPDATE 2502

/* Hostdependencies. */
#define NEBTYPE_HOSTDEPENDENCY_ADD 2600
#define NEBTYPE_HOSTDEPENDENCY_DELETE 2601
#define NEBTYPE_HOSTDEPENDENCY_UPDATE 2602

/* Hostescalation. */
#define NEBTYPE_HOSTESCALATION_ADD 2700
#define NEBTYPE_HOSTESCALATION_DELETE 2701
#define NEBTYPE_HOSTESCALATION_UPDATE 2702

/* Host groups. */
#define NEBTYPE_HOSTGROUP_ADD 2800
#define NEBTYPE_HOSTGROUP_DELETE 2801
#define NEBTYPE_HOSTGROUP_UPDATE 2802

/* Host group members. */
#define NEBTYPE_HOSTGROUPMEMBER_ADD 2900
#define NEBTYPE_HOSTGROUPMEMBER_DELETE 2901

/* Modules. */
#define NEBTYPE_MODULE_ADD 3000
#define NEBTYPE_MODULE_DELETE 3001

/* Parents. */
#define NEBTYPE_PARENT_ADD 3100
#define NEBTYPE_PARENT_DELETE 3101

/* Services. */
#define NEBTYPE_SERVICE_ADD 3200
#define NEBTYPE_SERVICE_DELETE 3201
#define NEBTYPE_SERVICE_UPDATE NEBTYPE_ADAPTIVESERVICE_UPDATE

/* Service custom variables. */
#define NEBTYPE_SERVICECUSTOMVARIABLE_ADD 3300
#define NEBTYPE_SERVICECUSTOMVARIABLE_DELETE 3301
#define NEBTYPE_SERVICECUSTOMVARIABLE_UPDATE 3302

/* Servicedependencies. */
#define NEBTYPE_SERVICEDEPENDENCY_ADD 3400
#define NEBTYPE_SERVICEDEPENDENCY_DELETE 3401
#define NEBTYPE_SERVICEDEPENDENCY_UPDATE 3402

/* Serviceescalation. */
#define NEBTYPE_SERVICEESCALATION_ADD 3500
#define NEBTYPE_SERVICEESCALATION_DELETE 3501
#define NEBTYPE_SERVICEESCALATION_UPDATE 3502

/* Service group. */
#define NEBTYPE_SERVICEGROUP_ADD 3600
#define NEBTYPE_SERVICEGROUP_DELETE 3601
#define NEBTYPE_SERVICEGROUP_UPDATE 3602

/*  Service group members. */
#define NEBTYPE_SERVICEGROUPMEMBER_ADD 3700
#define NEBTYPE_SERVICEGROUPMEMBER_DELETE 3701

/* Timeperiod. */
#define NEBTYPE_TIMEPERIOD_ADD 3800
#define NEBTYPE_TIMEPERIOD_DELETE 3801
#define NEBTYPE_TIMEPERIOD_UPDATE 3802

/* Severity. */
#define NEBTYPE_SEVERITY_ADD 3900
#define NEBTYPE_SEVERITY_DELETE 3901
#define NEBTYPE_SEVERITY_UPDATE 3902

/* Tag. */
#define NEBTYPE_TAG_ADD 4000
#define NEBTYPE_TAG_DELETE 4001
#define NEBTYPE_TAG_UPDATE 4002

/*
** Event flags.
*/
#define NEBFLAG_NONE 0
#define NEBFLAG_PROCESS_INITIATED                                          \
  1                              /* Event was initiated by Engine process. \
                                  */
#define NEBFLAG_USER_INITIATED 2 /* Event was initiated by a user request. */
#define NEBFLAG_MODULE_INITIATED \
  3 /* Event was initiated by an event broker module. */

/* Event attributes. */
#define NEBATTR_NONE 0

/* Termination. */
#define NEBATTR_SHUTDOWN_NORMAL (1 << 0)
#define NEBATTR_SHUTDOWN_ABNORMAL (1 << 1)
#define NEBATTR_RESTART_NORMAL (1 << 2)
#define NEBATTR_RESTART_ABNORMAL (1 << 3)
#define NEBATTR_BBDO3_ONLY (1 << 4)

/* Flapping. */
#define NEBATTR_FLAPPING_STOP_NORMAL 1
#define NEBATTR_FLAPPING_STOP_DISABLED \
  2 /* Flapping stopped because flap detection was disabled. */

/* Downtime. */
#define NEBATTR_DOWNTIME_STOP_NORMAL 1
#define NEBATTR_DOWNTIME_STOP_CANCELLED 2

template <typename R>
void broker_acknowledgement_data(R* data,
                                 const char* ack_author,
                                 const char* ack_data,
                                 int subtype,
                                 bool notify_contacts,
                                 bool persistent_comment);

void broker_adaptive_severity_data(int type,
                                   com::centreon::engine::severity* es);
void broker_adaptive_tag_data(int type, com::centreon::engine::tag* data);
void broker_adaptive_host_data(int type,
                               int flags,
                               com::centreon::engine::host* hst,
                               uint64_t modattr);
void broker_adaptive_service_data(int type,
                                  int flags,
                                  com::centreon::engine::service* svc,
                                  unsigned long modattr);
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
                         unsigned long comment_id);
template <typename R>
void broker_custom_variable(int type,
                            R* resource,
                            const std::string_view& varname,
                            const std::string_view& varvalue,
                            const struct timeval* timestamp);
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
                          unsigned long downtime_id);
void broker_external_command(int type,
                             int command_type,
                             char* command_args);
template <typename G>
void broker_group(int type, const G* group);

template <typename G, typename R>
void broker_group_member(int type, const R* object, const G* group);

int broker_host_check(int type,
                      const com::centreon::engine::host* hst,
                      int check_type,
                      const char* cmdline);
void broker_host_status(const com::centreon::engine::host* hst,
                        uint32_t attributes);
void broker_log_data_legacy(const char* data, time_t entry_time);
void broker_log_data(const char* data, time_t entry_time);
void broker_program_state(int type, int flags);
void broker_program_status();
void broker_relation_data(int type,
                          const com::centreon::engine::host* hst,
                          const com::centreon::engine::service* svc,
                          const com::centreon::engine::host* dep_hst,
                          const com::centreon::engine::service* dep_svc);
int broker_service_check(int type,
                         const com::centreon::engine::service* svc,
                         int check_type,
                         const char* cmdline);
void broker_service_status(const com::centreon::engine::service* svc,
                           uint32_t attributes);
struct timeval get_broker_timestamp(struct timeval const* timestamp);

void broker_bench(unsigned id,
                  const std::chrono::system_clock::time_point& mess_create);

struct nebstruct_agent_stats_data;

void broker_agent_stats(nebstruct_agent_stats_data& stats);

#endif /* !CCE_BROKER_HH */
