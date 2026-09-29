/**
 * Copyright 2026 Centreon
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

#ifndef CCB_BROKERRPC_LEGACY_COMMANDS_HH
#define CCB_BROKERRPC_LEGACY_COMMANDS_HH

#include <grpcpp/support/status.h>
#include <string_view>
#include <vector>

#include "broker/broker.pb.h"

namespace com::centreon::broker::legacy_commands {

/* Converters from the positional arguments of a legacy external command line
 * (what follows "NAME;") to the typed gRPC request Broker executes itself in
 * notification_mode=broker. Pure functions: they only parse, the RPC bodies
 * do the work. Each returns INVALID_ARGUMENT naming the faulty field. */

using fields = std::vector<std::string_view>;

/* Split args on ';' into at most n fields, the last one taking the rest of
 * the line (free text such as a comment may hold ';'). Missing fields are
 * absent from the result. */
fields split_args(std::string_view args, size_t n);

grpc::Status host_identifier(std::string_view args, HostIdentifier* id);
grpc::Status service_identifier(std::string_view args, ServiceIdentifier* id);
grpc::Status contact_identifier(std::string_view args, ContactIdentifier* id);
grpc::Status contactgroup_identifier(std::string_view args,
                                     ContactgroupIdentifier* id);
grpc::Status comment_identifier(std::string_view args, CommentIdentifier* id);
grpc::Status downtime_identifier(std::string_view args, DowntimeIdentifier* id);

/* ACKNOWLEDGE_{HOST,SVC}_PROBLEM;host[;svc];type;notify;persistent;author;data
 * type 2 = sticky (AckType::STICKY), anything else = normal. */
grpc::Status acknowledgement(std::string_view args,
                             bool service,
                             AcknowledgementRequest* req);
/* ADD_HOST_COMMENT;host;persistent;user;comment */
grpc::Status host_comment(std::string_view args, HostCommentRequest* req);
/* ADD_SVC_COMMENT;host;svc;persistent;user;comment */
grpc::Status service_comment(std::string_view args, ServiceCommentRequest* req);
/* SCHEDULE_{HOST,SVC}_DOWNTIME;host[;svc];start;end;fixed;trigger_id;duration;author;comment
 * Also the common part of the composed downtime commands (HOST_SVC,
 * AND_PROPAGATE*), which take the host form. */
grpc::Status schedule_downtime(std::string_view args,
                               bool service,
                               ScheduleDowntimeRequest* req);
/* SET_{HOST,SVC}_NOTIFICATION_NUMBER;host[;svc];number */
grpc::Status host_notification_number(std::string_view args,
                                      HostNotificationNumberRequest* req);
grpc::Status service_notification_number(std::string_view args,
                                         ServiceNotificationNumberRequest* req);
/* SEND_CUSTOM_{HOST,SVC}_NOTIFICATION;host[;svc];options;author;comment
 * options is the legacy bit mask 1 broadcast, 2 forced, 4 increment. */
grpc::Status host_custom_notification(std::string_view args,
                                      HostCustomNotificationRequest* req);
grpc::Status service_custom_notification(std::string_view args,
                                         ServiceCustomNotificationRequest* req);
/* CHANGE_{HOST,SVC}_NOTIFICATION_TIMEPERIOD;host[;svc];timeperiod */
grpc::Status host_notification_period(std::string_view args,
                                      HostNotificationPeriodRequest* req);
grpc::Status service_notification_period(std::string_view args,
                                         ServiceNotificationPeriodRequest* req);
/* CHANGE_CONTACT_{HOST,SVC}_NOTIFICATION_TIMEPERIOD;contact;timeperiod */
grpc::Status contact_notification_period(std::string_view args,
                                         ContactNotificationPeriodRequest* req);

}  // namespace com::centreon::broker::legacy_commands

#endif /* !CCB_BROKERRPC_LEGACY_COMMANDS_HH */
