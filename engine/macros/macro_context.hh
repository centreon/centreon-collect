/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#ifndef CCE_MACROS_MACRO_CONTEXT_HH
#define CCE_MACROS_MACRO_CONTEXT_HH

#include <array>
#include <optional>

#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/hostgroup.hh"
#include "com/centreon/engine/servicegroup.hh"
#include "common/notifications/notification_types.hh"
#include "engine/macros/defines.hh"

namespace com::centreon::engine {

class notifier;

/**
 * @brief The values of the $NOTIFICATION*$ macros, given by the notification
 * being sent. In notification_mode=broker, they come from Broker which made
 * the decision: the notifier does not know them.
 */
struct notification_values {
  /* The host or service the notification is about. */
  const notifier* resource = nullptr;
  com::centreon::common::notifications::reason_type type;
  uint64_t id = 0;
  uint32_t number;
  bool is_escalated;
  std::string recipients;
  std::string author;
  std::string author_name;
  std::string author_alias;
  std::string comment;
};

/**
 * @brief The context in which the macros are expanded: the objects they are
 * about, the notification being sent and the arguments of the command.
 */
class macro_context {
 public:
  void set_host(host* hst);
  void set_service(service* svc);
  void set_contact(contact* cntct);
  void clear_argv();
  void reset();

  std::array<std::string, MAX_COMMAND_ARGUMENTS> argv;
  host* host_ptr = nullptr;
  hostgroup* hostgroup_ptr = nullptr;
  service* service_ptr = nullptr;
  servicegroup* servicegroup_ptr = nullptr;
  contact* contact_ptr = nullptr;
  contactgroup* contactgroup_ptr = nullptr;

  /* Only set while a notification is sent. */
  std::optional<notification_values> notification;

  /* Cache of the $TOTAL*$ macros, they depend on contact_ptr. */
  std::optional<std::array<uint32_t, MACRO_SUMMARY_COUNT>> summary;
  /* Cache of the $TOTALHOSTSERVICES*$ macros of host_ptr. */
  std::optional<std::array<uint32_t, MACRO_HOST_SERVICES_COUNT>> host_services;
};

}  // namespace com::centreon::engine

#endif  // !CCE_MACROS_MACRO_CONTEXT_HH
