/**
 * Copyright 2009-2011,2015-2024 Centreon
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

#include "com/centreon/broker/neb/set_log_data.hh"
#include <absl/strings/str_split.h>
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/log_entry.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using com::centreon::common::log_v2::log_v2;

/**
 *  Extract the first element of a log string.
 */
static char* log_extract_first(char* str, char** lasts) {
  char* data(strtok_r(str, ";", lasts));
  if (!data)
    throw msg_fmt("log: data extraction failed");
  return data;
}

/**
 *  Extract following elements of a log string.
 */
static char* log_extract(char** lasts) {
  char* data(strtok_r(nullptr, ";", lasts));
  if (!data)
    throw msg_fmt("log: data extraction failed");
  return data;
}

/**
 * @brief Get the id of a log status.
 *
 * @param status A string corresponding to the state of the resource.
 *
 * @return A status code.
 */
static int status_id(const std::string_view& status) {
  int retval;
  if (status == "DOWN" || status == "WARNING")
    retval = 1;
  else if (status == "UNREACHABLE" || status == "CRITICAL")
    retval = 2;
  else if (status == "UNKNOWN")
    retval = 3;
  else if (status == "PENDING")
    retval = 4;
  else
    retval = 0;
  return retval;
}

/**
 *  Get the notification status of a log.
 */
static int notification_status_id(const std::string_view& status) {
  int retval;
  size_t pos_start = status.find_first_of('(');
  if (pos_start != std::string::npos) {
    size_t pos_end = status.find_first_of(')', pos_start);
    std::string_view nstatus = status.substr(pos_start, pos_end - pos_start);
    retval = status_id(nstatus);
  } else
    retval = status_id(status);
  return retval;
}

/**
 *  Get the id of a log type.
 */
static LogEntry_LogType type_id(const std::string_view& type) {
  LogEntry_LogType id;
  if (type == "HARD")
    id = LogEntry_LogType_HARD;
  else
    id = LogEntry_LogType_SOFT;
  return id;
}

/**
 *  Extract Nagios-formated log data to the C++ object.
 */
void neb::set_log_data(neb::log_entry& le, char const* log_data) {
  // Duplicate string so that we can split it with strtok_r.
  char* datadup(strdup(log_data));
  if (!datadup)
    throw msg_fmt("log: data extraction failed");

  try {
    char* lasts;

    // First part is the log description.
    lasts = datadup + strcspn(datadup, ":");
    if (*lasts) {
      *lasts = '\0';
      lasts = lasts + 1 + strspn(lasts + 1, " ");
    }
    if (!strcmp(datadup, "SERVICE ALERT")) {
      le.msg_type = log_entry::service_alert;
      le.host_name = log_extract_first(lasts, &lasts);
      le.service_description = log_extract(&lasts);
      le.status = status_id(log_extract(&lasts));
      le.log_type = type_id(log_extract(&lasts));
      le.retry = strtol(log_extract(&lasts), nullptr, 10);
      le.output = log_extract(&lasts);
    } else if (!strcmp(datadup, "HOST ALERT")) {
      le.msg_type = log_entry::host_alert;
      le.host_name = log_extract_first(lasts, &lasts);
      le.status = status_id(log_extract(&lasts));
      le.log_type = type_id(log_extract(&lasts));
      le.retry = strtol(log_extract(&lasts), nullptr, 10);
      le.output = log_extract(&lasts);
    } else if (!strcmp(datadup, "SERVICE NOTIFICATION")) {
      le.msg_type = log_entry::service_notification;
      le.notification_contact = log_extract_first(lasts, &lasts);
      le.host_name = log_extract(&lasts);
      le.service_description = log_extract(&lasts);
      le.status = notification_status_id(log_extract(&lasts));
      le.notification_cmd = log_extract(&lasts);
      le.output = log_extract(&lasts);
    } else if (!strcmp(datadup, "HOST NOTIFICATION")) {
      le.msg_type = log_entry::host_notification;
      le.notification_contact = log_extract_first(lasts, &lasts);
      le.host_name = log_extract(&lasts);
      le.status = notification_status_id(log_extract(&lasts));
      le.notification_cmd = log_extract(&lasts);
      le.output = log_extract(&lasts);
    } else if (!strcmp(datadup, "INITIAL HOST STATE")) {
      le.msg_type = log_entry::host_initial_state;
      le.host_name = log_extract_first(lasts, &lasts);
      le.status = status_id(log_extract(&lasts));
      le.log_type = type_id(log_extract(&lasts));
      le.retry = strtol(log_extract(&lasts), nullptr, 10);
      le.output = log_extract(&lasts);
    } else if (!strcmp(datadup, "INITIAL SERVICE STATE")) {
      le.msg_type = log_entry::service_initial_state;
      le.host_name = log_extract_first(lasts, &lasts);
      le.service_description = log_extract(&lasts);
      le.status = status_id(log_extract(&lasts));
      le.log_type = type_id(log_extract(&lasts));
      le.retry = strtol(log_extract(&lasts), nullptr, 10);
      le.output = log_extract(&lasts);
    } else if (!strcmp(datadup, "EXTERNAL COMMAND")) {
      char* data(log_extract_first(lasts, &lasts));
      if (!strcmp(data, "ACKNOWLEDGE_SVC_PROBLEM")) {
        le.msg_type = log_entry::service_acknowledge_problem;
        le.host_name = log_extract(&lasts);
        le.service_description = log_extract(&lasts);
        log_extract(&lasts);
        log_extract(&lasts);
        log_extract(&lasts);
        le.notification_contact = log_extract(&lasts);
        le.output = log_extract(&lasts);
      } else if (!strcmp(data, "ACKNOWLEDGE_HOST_PROBLEM")) {
        le.msg_type = log_entry::host_acknowledge_problem;
        le.host_name = log_extract(&lasts);
        log_extract(&lasts);
        log_extract(&lasts);
        log_extract(&lasts);
        le.notification_contact = log_extract(&lasts);
        le.output = log_extract(&lasts);
      } else {
        le.msg_type = log_entry::other;
        le.output = log_data;
      }
    } else if (!strcmp(datadup, "HOST EVENT HANDLER")) {
      le.msg_type = log_entry::host_event_handler;
      le.host_name = log_extract_first(lasts, &lasts);
      le.status = status_id(log_extract(&lasts));
      le.log_type = type_id(log_extract(&lasts));
      le.retry = strtol(log_extract(&lasts), nullptr, 10);
      le.output = log_extract(&lasts);
    } else if (!strcmp(datadup, "SERVICE EVENT HANDLER")) {
      le.msg_type = log_entry::service_event_handler;
      le.host_name = log_extract_first(lasts, &lasts);
      le.service_description = log_extract(&lasts);
      le.status = status_id(log_extract(&lasts));
      le.log_type = type_id(log_extract(&lasts));
      le.retry = strtol(log_extract(&lasts), nullptr, 10);
      le.output = log_extract(&lasts);
    } else if (!strcmp(datadup, "GLOBAL HOST EVENT HANDLER")) {
      le.msg_type = log_entry::global_host_event_handler;
      le.host_name = log_extract_first(lasts, &lasts);
      le.status = status_id(log_extract(&lasts));
      le.log_type = type_id(log_extract(&lasts));
      le.retry = strtol(log_extract(&lasts), nullptr, 10);
      le.output = log_extract(&lasts);
    } else if (!strcmp(datadup, "GLOBAL SERVICE EVENT HANDLER")) {
      le.msg_type = log_entry::global_service_event_handler;
      le.host_name = log_extract_first(lasts, &lasts);
      le.service_description = log_extract(&lasts);
      le.status = status_id(log_extract(&lasts));
      le.log_type = type_id(log_extract(&lasts));
      le.retry = strtol(log_extract(&lasts), nullptr, 10);
      le.output = log_extract(&lasts);
    } else if (!strcmp(datadup, "Warning")) {
      le.msg_type = log_entry::warning;
      le.output = lasts;
    } else {
      le.msg_type = log_entry::other;
      le.output = log_data;
    }
  } catch (...) {
  }
  free(datadup);

  // Set host and service IDs.
  le.host_id = engine::get_host_id(le.host_name);
  le.service_id = engine::get_service_id(le.host_name, le.service_description);
}

#define test_fail(name)                                            \
  if (ait == args.end()) {                                         \
    logger->error("Missing " name " in log message '{}'", output); \
    return false;                                                  \
  }

#define test_fail_and_not_empty(name)                              \
  if (ait == args.end()) {                                         \
    logger->error("Missing " name " in log message '{}'", output); \
    return false;                                                  \
  }                                                                \
  if (ait->empty()) {                                              \
    return false;                                                  \
  }

/**
 *  Extract Nagios-formated log data to the C++ object.
 *
 *  Return true on success.
 */
bool neb::set_pb_log_data(neb::pb_log_entry& le, const std::string& output) {
  // Duplicate string so that we can split it with strtok_r.
  auto& le_obj = le.mut_obj();

  /**
   * @brief The only goal of this internal class is to fill host_id and
   * service_id when destructor is called ie on each returns
   * macro used in this function can do a return false
   *
   */
  class fill_obj_on_exit {
    LogEntry& _to_fill;

   public:
    fill_obj_on_exit(LogEntry& to_fill) : _to_fill(to_fill) {}
    ~fill_obj_on_exit() {
      if (!_to_fill.host_name().empty()) {
        _to_fill.set_host_id(engine::get_host_id(_to_fill.host_name()));
        if (!_to_fill.service_description().empty()) {
          _to_fill.set_service_id(engine::get_service_id(
              _to_fill.host_name(), _to_fill.service_description()));
        }
      }
    }
  };

  // try to fill host_id and service_id whereever function exits
  fill_obj_on_exit on_exit_executor(le_obj);

  // First part is the log description.
  auto s = absl::StrSplit(output, absl::MaxSplits(':', 1));
  auto it = s.begin();
  auto typ = *it;
  ++it;
  auto lasts = *it;
  lasts = absl::StripLeadingAsciiWhitespace(lasts);
  auto args = absl::StrSplit(lasts, ';');
  auto ait = args.begin();
  auto logger = log_v2::instance().get(log_v2::NEB);

  if (typ == "SERVICE ALERT") {
    le_obj.set_msg_type(LogEntry_MsgType_SERVICE_ALERT);
    test_fail("host name");
    le_obj.set_host_name(ait->data(), ait->size());
    ++ait;

    test_fail("service description");
    le_obj.set_service_description(ait->data(), ait->size());
    ++ait;

    test_fail("status");
    le_obj.set_status(status_id(*ait));
    ++ait;

    test_fail("log type");
    le_obj.set_type(type_id(*ait));
    ++ait;

    test_fail("retry value");
    int retry;
    if (!absl::SimpleAtoi(*ait, &retry)) {
      logger->error(
          "Retry value in log message should be an integer and not '{}'",
          fmt::string_view(ait->data(), ait->size()));
      return false;
    }
    le_obj.set_retry(retry);
    ++ait;

    test_fail_and_not_empty("output");
    le_obj.set_output(ait->data(), ait->size());
  } else if (typ == "HOST ALERT") {
    le_obj.set_msg_type(LogEntry_MsgType_HOST_ALERT);
    test_fail("host name");
    le_obj.set_host_name(ait->data(), ait->size());
    ++ait;

    test_fail("status");
    le_obj.set_status(status_id(*ait));
    ++ait;

    test_fail("log type");
    le_obj.set_type(type_id(*ait));
    ++ait;

    test_fail("retry value");
    int retry;
    if (!absl::SimpleAtoi(*ait, &retry)) {
      logger->error(
          "Retry value in log message should be an integer and not '{}'",
          fmt::string_view(ait->data(), ait->size()));
      return false;
    }
    le_obj.set_retry(retry);
    ++ait;

    test_fail_and_not_empty("output");
    le_obj.set_output(ait->data(), ait->size());
  } else if (typ == "SERVICE NOTIFICATION") {
    le_obj.set_msg_type(LogEntry_MsgType_SERVICE_NOTIFICATION);

    test_fail("notification contact");
    le_obj.set_notification_contact(ait->data(), ait->size());
    ++ait;

    test_fail("host name");
    le_obj.set_host_name(ait->data(), ait->size());
    ++ait;

    test_fail("service description");
    le_obj.set_service_description(ait->data(), ait->size());
    ++ait;

    test_fail("status");
    le_obj.set_status(notification_status_id(*ait));
    ++ait;

    test_fail("notification command");
    le_obj.set_notification_cmd(ait->data(), ait->size());
    ++ait;

    test_fail_and_not_empty("output");
    le_obj.set_output(ait->data(), ait->size());
  } else if (typ == "HOST NOTIFICATION") {
    le_obj.set_msg_type(LogEntry_MsgType_HOST_NOTIFICATION);

    test_fail("notification contact");
    le_obj.set_notification_contact(ait->data(), ait->size());
    ++ait;

    test_fail("host name");
    le_obj.set_host_name(ait->data(), ait->size());
    ++ait;

    test_fail("status");
    le_obj.set_status(notification_status_id(*ait));
    ++ait;

    test_fail("notification command");
    le_obj.set_notification_cmd(ait->data(), ait->size());
    ++ait;

    test_fail_and_not_empty("output");
    le_obj.set_output(ait->data(), ait->size());
  } else if (typ == "INITIAL HOST STATE") {
    le_obj.set_msg_type(LogEntry_MsgType_HOST_INITIAL_STATE);

    test_fail("host name");
    le_obj.set_host_name(ait->data(), ait->size());
    ++ait;

    test_fail("status");
    le_obj.set_status(notification_status_id(*ait));
    ++ait;

    test_fail("log type");
    le_obj.set_type(type_id(*ait));
    ++ait;

    test_fail("retry value");
    int retry;
    if (!absl::SimpleAtoi(*ait, &retry)) {
      logger->error(
          "Retry value in log message should be an integer and not '{}'",
          fmt::string_view(ait->data(), ait->size()));
      return false;
    }
    le_obj.set_retry(retry);
    ++ait;

    test_fail_and_not_empty("output");
    le_obj.set_output(ait->data(), ait->size());
  } else if (typ == "INITIAL SERVICE STATE") {
    le_obj.set_msg_type(LogEntry_MsgType_SERVICE_INITIAL_STATE);

    test_fail("host name");
    le_obj.set_host_name(ait->data(), ait->size());
    ++ait;

    test_fail("service description");
    le_obj.set_service_description(ait->data(), ait->size());
    ++ait;

    test_fail("status");
    le_obj.set_status(status_id(*ait));
    ++ait;

    test_fail("log type");
    le_obj.set_type(type_id(*ait));
    ++ait;

    test_fail("retry value");
    int retry;
    if (!absl::SimpleAtoi(*ait, &retry)) {
      logger->error(
          "Retry value in log message should be an integer and not '{}'",
          fmt::string_view(ait->data(), ait->size()));
      return false;
    }
    le_obj.set_retry(retry);
    ++ait;

    test_fail_and_not_empty("output");
    le_obj.set_output(ait->data(), ait->size());
  } else if (typ == "EXTERNAL COMMAND") {
    test_fail("acknowledge type");
    std::string_view data = *ait;
    ++ait;
    if (data == "ACKNOWLEDGE_SVC_PROBLEM") {
      le_obj.set_msg_type(LogEntry_MsgType_SERVICE_ACKNOWLEDGE_PROBLEM);
      test_fail("host name");
      le_obj.set_host_name(ait->data(), ait->size());
      ++ait;

      test_fail("service description");
      le_obj.set_service_description(ait->data(), ait->size());
      ++ait;

      for (int i = 0; i < 3; i++) {
        test_fail("data");
        ++ait;
      }

      test_fail("notification contact");
      le_obj.set_notification_contact(ait->data(), ait->size());
      ++ait;

      test_fail_and_not_empty("output");
      le_obj.set_output(ait->data(), ait->size());
    } else if (data == "ACKNOWLEDGE_HOST_PROBLEM") {
      le_obj.set_msg_type(LogEntry_MsgType_HOST_ACKNOWLEDGE_PROBLEM);

      test_fail("host name");
      le_obj.set_host_name(ait->data(), ait->size());
      ++ait;

      for (int i = 0; i < 3; i++) {
        test_fail("data");
        ++ait;
      }

      test_fail("notification contact");
      le_obj.set_notification_contact(ait->data(), ait->size());
      ++ait;

      test_fail_and_not_empty("output");
      le_obj.set_output(ait->data(), ait->size());
    } else {
      le_obj.set_msg_type(LogEntry_MsgType_OTHER);
      le_obj.set_output(output);
    }
  } else if (typ == "HOST EVENT HANDLER") {
    le_obj.set_msg_type(LogEntry_MsgType_HOST_EVENT_HANDLER);

    test_fail("host name");
    le_obj.set_host_name(ait->data(), ait->size());
    ++ait;

    test_fail("status");
    le_obj.set_status(status_id(*ait));
    ++ait;

    test_fail("log type");
    le_obj.set_type(type_id(*ait));
    ++ait;

    test_fail("retry value");
    int retry;
    if (!absl::SimpleAtoi(*ait, &retry)) {
      logger->error(
          "Retry value in log message should be an integer and not '{}'",
          fmt::string_view(ait->data(), ait->size()));
      return false;
    }
    le_obj.set_retry(retry);
    ++ait;

    test_fail_and_not_empty("output");
    le_obj.set_output(ait->data(), ait->size());
  } else if (typ == "SERVICE EVENT HANDLER") {
    le_obj.set_msg_type(LogEntry_MsgType_SERVICE_EVENT_HANDLER);

    test_fail("host name");
    le_obj.set_host_name(ait->data(), ait->size());
    ++ait;

    test_fail("service description");
    le_obj.set_service_description(ait->data(), ait->size());
    ++ait;

    test_fail("status");
    le_obj.set_status(status_id(*ait));
    ++ait;

    test_fail("log type");
    le_obj.set_type(type_id(*ait));
    ++ait;

    test_fail("retry value");
    int retry;
    if (!absl::SimpleAtoi(*ait, &retry)) {
      logger->error(
          "Retry value in log message should be an integer and not '{}'",
          fmt::string_view(ait->data(), ait->size()));
      return false;
    }
    le_obj.set_retry(retry);
    ++ait;

    test_fail_and_not_empty("output");
    le_obj.set_output(ait->data(), ait->size());
  } else if (typ == "GLOBAL HOST EVENT HANDLER") {
    le_obj.set_msg_type(LogEntry_MsgType_GLOBAL_HOST_EVENT_HANDLER);

    test_fail("host name");
    le_obj.set_host_name(ait->data(), ait->size());
    ++ait;

    test_fail("status");
    le_obj.set_status(status_id(*ait));
    ++ait;

    test_fail("log type");
    le_obj.set_type(type_id(*ait));
    ++ait;

    test_fail("retry value");
    int retry;
    if (!absl::SimpleAtoi(*ait, &retry)) {
      logger->error(
          "Retry value in log message should be an integer and not '{}'",
          fmt::string_view(ait->data(), ait->size()));
      return false;
    }
    le_obj.set_retry(retry);
    ++ait;

    test_fail_and_not_empty("output");
    le_obj.set_output(ait->data(), ait->size());
  } else if (typ == "GLOBAL SERVICE EVENT HANDLER") {
    le_obj.set_msg_type(LogEntry_MsgType_GLOBAL_SERVICE_EVENT_HANDLER);

    test_fail("host name");
    le_obj.set_host_name(ait->data(), ait->size());
    ++ait;

    test_fail("service description");
    le_obj.set_service_description(ait->data(), ait->size());
    ++ait;

    test_fail("status");
    le_obj.set_status(status_id(*ait));
    ++ait;

    test_fail("log type");
    le_obj.set_type(type_id(*ait));
    ++ait;

    test_fail("retry value");
    int retry;
    if (!absl::SimpleAtoi(*ait, &retry)) {
      logger->error(
          "Retry value in log message should be an integer and not '{}'",
          fmt::string_view(ait->data(), ait->size()));
      return false;
    }
    le_obj.set_retry(retry);
    ++ait;

    test_fail_and_not_empty("output");
    le_obj.set_output(ait->data(), ait->size());
  } else if (typ == "Warning") {
    le_obj.set_msg_type(LogEntry_MsgType_WARNING);
    le_obj.set_output(lasts.data(), lasts.size());
  } else {
    le_obj.set_msg_type(LogEntry_MsgType_OTHER);
    le_obj.set_output(output);
  }

  return true;
}
