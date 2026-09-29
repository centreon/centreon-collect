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

#include "broker/core/brokerrpc/legacy_commands.hh"

#include <absl/strings/numbers.h>
#include <fmt/format.h>

namespace com::centreon::broker::legacy_commands {

namespace {

grpc::Status missing(std::string_view what) {
  return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                      fmt::format("missing argument '{}'", what));
}

/* Fetch field i as a non empty string, or fail naming it. */
grpc::Status text(const fields& f,
                  size_t i,
                  std::string_view what,
                  std::string* out) {
  if (i >= f.size() || f[i].empty())
    return missing(what);
  out->assign(f[i]);
  return grpc::Status::OK;
}

template <typename T>
grpc::Status integer(const fields& f, size_t i, std::string_view what, T* out) {
  if (i >= f.size())
    return missing(what);
  if (!absl::SimpleAtoi(f[i], out))
    return grpc::Status(
        grpc::StatusCode::INVALID_ARGUMENT,
        fmt::format("argument '{}' must be an integer, got '{}'", what, f[i]));
  return grpc::Status::OK;
}

/* Legacy booleans are integers: 0 = false, anything else = true. */
grpc::Status boolean(const fields& f,
                     size_t i,
                     std::string_view what,
                     bool* out) {
  int64_t v = 0;
  grpc::Status s = integer(f, i, what, &v);
  if (s.ok())
    *out = v != 0;
  return s;
}

/* Fill a ServiceIdentifier from fields i (host) and i+1 (description). */
grpc::Status service_at(const fields& f, size_t i, ServiceIdentifier* id) {
  std::string v;
  if (auto s = text(f, i, "host_name", &v); !s.ok())
    return s;
  id->set_host_name(v);
  if (auto s = text(f, i + 1, "service_description", &v); !s.ok())
    return s;
  id->set_description(v);
  return grpc::Status::OK;
}

}  // namespace

fields split_args(std::string_view args, size_t n) {
  fields out;
  if (args.empty() || n == 0)
    return out;
  while (out.size() + 1 < n) {
    size_t sep = args.find(';');
    if (sep == std::string_view::npos)
      break;
    out.push_back(args.substr(0, sep));
    args.remove_prefix(sep + 1);
  }
  out.push_back(args);
  return out;
}

grpc::Status host_identifier(std::string_view args, HostIdentifier* id) {
  fields f = split_args(args, 1);
  std::string v;
  if (auto s = text(f, 0, "host_name", &v); !s.ok())
    return s;
  id->set_host_name(v);
  return grpc::Status::OK;
}

grpc::Status service_identifier(std::string_view args, ServiceIdentifier* id) {
  return service_at(split_args(args, 2), 0, id);
}

grpc::Status contact_identifier(std::string_view args, ContactIdentifier* id) {
  fields f = split_args(args, 1);
  std::string v;
  if (auto s = text(f, 0, "contact_name", &v); !s.ok())
    return s;
  id->set_name(v);
  return grpc::Status::OK;
}

grpc::Status contactgroup_identifier(std::string_view args,
                                     ContactgroupIdentifier* id) {
  fields f = split_args(args, 1);
  std::string v;
  if (auto s = text(f, 0, "contactgroup_name", &v); !s.ok())
    return s;
  id->set_name(v);
  return grpc::Status::OK;
}

grpc::Status comment_identifier(std::string_view args, CommentIdentifier* id) {
  uint64_t v = 0;
  if (auto s = integer(split_args(args, 1), 0, "comment_id", &v); !s.ok())
    return s;
  id->set_internal_id(v);
  return grpc::Status::OK;
}

grpc::Status downtime_identifier(std::string_view args,
                                 DowntimeIdentifier* id) {
  uint64_t v = 0;
  if (auto s = integer(split_args(args, 1), 0, "downtime_id", &v); !s.ok())
    return s;
  id->set_downtime_id(v);
  return grpc::Status::OK;
}

grpc::Status acknowledgement(std::string_view args,
                             bool service,
                             AcknowledgementRequest* req) {
  size_t i = service ? 2 : 1;
  fields f = split_args(args, i + 5);
  std::string v;
  if (auto s = text(f, 0, "host_name", &v); !s.ok())
    return s;
  req->set_host_name(v);
  if (service) {
    if (auto s = text(f, 1, "service_description", &v); !s.ok())
      return s;
    req->set_service_desc(v);
  }
  int32_t type = 0;
  if (auto s = integer(f, i, "type", &type); !s.ok())
    return s;
  req->set_type(type == AckType::STICKY ? AcknowledgementRequest::STICKY
                                        : AcknowledgementRequest::NORMAL);
  bool b = false;
  if (auto s = boolean(f, i + 1, "notify", &b); !s.ok())
    return s;
  req->set_notify(b);
  if (auto s = boolean(f, i + 2, "persistent", &b); !s.ok())
    return s;
  req->set_persistent(b);
  if (auto s = text(f, i + 3, "author", &v); !s.ok())
    return s;
  req->set_ack_author(v);
  if (auto s = text(f, i + 4, "comment", &v); !s.ok())
    return s;
  req->set_ack_data(v);
  return grpc::Status::OK;
}

grpc::Status host_comment(std::string_view args, HostCommentRequest* req) {
  fields f = split_args(args, 4);
  std::string v;
  if (auto s = text(f, 0, "host_name", &v); !s.ok())
    return s;
  req->mutable_host()->set_host_name(v);
  bool b = false;
  if (auto s = boolean(f, 1, "persistent", &b); !s.ok())
    return s;
  req->set_persistent(b);
  if (auto s = text(f, 2, "user", &v); !s.ok())
    return s;
  req->set_user(v);
  if (auto s = text(f, 3, "comment", &v); !s.ok())
    return s;
  req->set_comment_data(v);
  return grpc::Status::OK;
}

grpc::Status service_comment(std::string_view args,
                             ServiceCommentRequest* req) {
  fields f = split_args(args, 5);
  if (auto s = service_at(f, 0, req->mutable_service()); !s.ok())
    return s;
  bool b = false;
  if (auto s = boolean(f, 2, "persistent", &b); !s.ok())
    return s;
  req->set_persistent(b);
  std::string v;
  if (auto s = text(f, 3, "user", &v); !s.ok())
    return s;
  req->set_user(v);
  if (auto s = text(f, 4, "comment", &v); !s.ok())
    return s;
  req->set_comment_data(v);
  return grpc::Status::OK;
}

grpc::Status schedule_downtime(std::string_view args,
                               bool service,
                               ScheduleDowntimeRequest* req) {
  size_t i = service ? 2 : 1;
  fields f = split_args(args, i + 7);
  std::string v;
  if (auto s = text(f, 0, "host_name", &v); !s.ok())
    return s;
  req->set_host_name(v);
  if (service) {
    if (auto s = text(f, 1, "service_description", &v); !s.ok())
      return s;
    req->set_service_description(v);
    req->set_type(ScheduleDowntimeRequest::SERVICE);
  } else
    req->set_type(ScheduleDowntimeRequest::HOST);
  int64_t t = 0;
  if (auto s = integer(f, i, "start_time", &t); !s.ok())
    return s;
  req->set_start_time(t);
  if (auto s = integer(f, i + 1, "end_time", &t); !s.ok())
    return s;
  req->set_end_time(t);
  bool b = false;
  if (auto s = boolean(f, i + 2, "fixed", &b); !s.ok())
    return s;
  req->set_fixed(b);
  uint64_t u = 0;
  if (auto s = integer(f, i + 3, "trigger_id", &u); !s.ok())
    return s;
  req->set_triggered_by(u);
  uint32_t d = 0;
  if (auto s = integer(f, i + 4, "duration", &d); !s.ok())
    return s;
  req->set_duration(d);
  if (auto s = text(f, i + 5, "author", &v); !s.ok())
    return s;
  req->set_author(v);
  if (auto s = text(f, i + 6, "comment", &v); !s.ok())
    return s;
  req->set_comment_data(v);
  return grpc::Status::OK;
}

grpc::Status host_notification_number(std::string_view args,
                                      HostNotificationNumberRequest* req) {
  fields f = split_args(args, 2);
  std::string v;
  if (auto s = text(f, 0, "host_name", &v); !s.ok())
    return s;
  req->mutable_host()->set_host_name(v);
  uint32_t n = 0;
  if (auto s = integer(f, 1, "number", &n); !s.ok())
    return s;
  req->set_number(n);
  return grpc::Status::OK;
}

grpc::Status service_notification_number(
    std::string_view args,
    ServiceNotificationNumberRequest* req) {
  fields f = split_args(args, 3);
  if (auto s = service_at(f, 0, req->mutable_service()); !s.ok())
    return s;
  uint32_t n = 0;
  if (auto s = integer(f, 2, "number", &n); !s.ok())
    return s;
  req->set_number(n);
  return grpc::Status::OK;
}

namespace {
/* Common tail of SEND_CUSTOM_*_NOTIFICATION: options;author;comment. */
template <typename Req>
grpc::Status custom_tail(const fields& f, size_t i, Req* req) {
  uint32_t options = 0;
  if (auto s = integer(f, i, "options", &options); !s.ok())
    return s;
  req->set_broadcast(options & 1);
  req->set_forced(options & 2);
  req->set_increment(options & 4);
  std::string v;
  if (auto s = text(f, i + 1, "author", &v); !s.ok())
    return s;
  req->set_author(v);
  if (auto s = text(f, i + 2, "comment", &v); !s.ok())
    return s;
  req->set_comment(v);
  return grpc::Status::OK;
}
}  // namespace

grpc::Status host_custom_notification(std::string_view args,
                                      HostCustomNotificationRequest* req) {
  fields f = split_args(args, 4);
  std::string v;
  if (auto s = text(f, 0, "host_name", &v); !s.ok())
    return s;
  req->mutable_host()->set_host_name(v);
  return custom_tail(f, 1, req);
}

grpc::Status service_custom_notification(
    std::string_view args,
    ServiceCustomNotificationRequest* req) {
  fields f = split_args(args, 5);
  if (auto s = service_at(f, 0, req->mutable_service()); !s.ok())
    return s;
  return custom_tail(f, 2, req);
}

grpc::Status host_notification_period(std::string_view args,
                                      HostNotificationPeriodRequest* req) {
  fields f = split_args(args, 2);
  std::string v;
  if (auto s = text(f, 0, "host_name", &v); !s.ok())
    return s;
  req->mutable_host()->set_host_name(v);
  if (auto s = text(f, 1, "timeperiod", &v); !s.ok())
    return s;
  req->set_timeperiod(v);
  return grpc::Status::OK;
}

grpc::Status service_notification_period(
    std::string_view args,
    ServiceNotificationPeriodRequest* req) {
  fields f = split_args(args, 3);
  if (auto s = service_at(f, 0, req->mutable_service()); !s.ok())
    return s;
  std::string v;
  if (auto s = text(f, 2, "timeperiod", &v); !s.ok())
    return s;
  req->set_timeperiod(v);
  return grpc::Status::OK;
}

grpc::Status contact_notification_period(
    std::string_view args,
    ContactNotificationPeriodRequest* req) {
  fields f = split_args(args, 2);
  std::string v;
  if (auto s = text(f, 0, "contact_name", &v); !s.ok())
    return s;
  req->mutable_contact()->set_name(v);
  if (auto s = text(f, 1, "timeperiod", &v); !s.ok())
    return s;
  req->set_timeperiod(v);
  return grpc::Status::OK;
}

}  // namespace com::centreon::broker::legacy_commands
