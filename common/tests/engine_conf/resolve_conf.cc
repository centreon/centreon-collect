/**
 * Copyright 2024-2026 Centreon (https://www.centreon.com/)
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

#include <gtest/gtest.h>

#include "common/engine_conf/state.pb.h"
#include "common/engine_conf/state_helper.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::engine::configuration;
using com::centreon::common::log_v2::log_v2;

/**
 * These tests cover the configuration-level validation that was migrated out of
 * the Engine runtime objects' resolve()/applier resolve_object() into
 * state_helper::resolve() + the per-object *_helper::resolve(). They build a
 * State, expand it and resolve it, then assert on the accumulated
 * warnings/errors (state_helper::resolve never throws).
 */
class Pb_Resolve : public ::testing::Test {
 public:
  static void SetUpTestSuite() { log_v2::load("resolve-tests"); }
  static void TearDownTestSuite() { log_v2::unload(); }

 protected:
  // Add a defined command to the State.
  static void add_command(State& s, const std::string& name) {
    Command* c = s.add_commands();
    c->set_command_name(name);
    c->set_command_line("true");
  }

  // Add a defined timeperiod to the State.
  static void add_timeperiod(State& s, const std::string& name) {
    Timeperiod* t = s.add_timeperiods();
    t->set_timeperiod_name(name);
    t->set_alias(name);
  }

  // Add a defined host to the State.
  static void add_host(State& s, const std::string& name) {
    Host* h = s.add_hosts();
    h->set_host_name(name);
  }

  // Add a defined service to the State.
  static void add_service(State& s,
                          const std::string& host,
                          const std::string& description) {
    Service* svc = s.add_services();
    svc->set_host_name(host);
    svc->set_service_description(description);
  }

  // Add a fully valid contact (existing periods + commands) to the State.
  static Contact* add_valid_contact(State& s) {
    Contact* c = s.add_contacts();
    c->set_contact_name("admin");
    c->set_host_notification_period("24x7");
    c->set_service_notification_period("24x7");
    c->mutable_host_notification_commands()->add_data("cmd");
    c->mutable_service_notification_commands()->add_data("cmd");
    return c;
  }

  // Build a State with the command "cmd" and timeperiod "24x7" defined.
  static State base_state() {
    State s;
    add_command(s, "cmd");
    add_timeperiod(s, "24x7");
    return s;
  }
};

// A fully valid contact resolves without any warning or error.
TEST_F(Pb_Resolve, ContactValid) {
  State s = base_state();
  add_valid_contact(s);

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A contact with no notification command and no notification period gets:
//  * error 1 => no service notification command
//  * error 2 => no host notification command
//  * warning 1 => no service notification period
//  * warning 2 => no host notification period
TEST_F(Pb_Resolve, ContactNoNotification) {
  State s = base_state();
  Contact* c = s.add_contacts();
  c->set_contact_name("test");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 2u);
  ASSERT_EQ(err.config_errors, 2u);
}

// A non-existing service notification period is a single error.
TEST_F(Pb_Resolve, NonExistingServiceNotificationTimeperiod) {
  State s = base_state();
  Contact* c = add_valid_contact(s);
  c->set_service_notification_period("non_existing_period");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A non-existing host notification period is a single error.
TEST_F(Pb_Resolve, NonExistingHostNotificationTimeperiod) {
  State s = base_state();
  Contact* c = add_valid_contact(s);
  c->set_host_notification_period("non_existing_period");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A non-existing service notification command is a single error.
TEST_F(Pb_Resolve, NonExistingServiceCommand) {
  State s = base_state();
  Contact* c = add_valid_contact(s);
  c->mutable_service_notification_commands()->add_data("non_existing_command");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A non-existing host notification command is a single error.
TEST_F(Pb_Resolve, NonExistingHostCommand) {
  State s = base_state();
  Contact* c = add_valid_contact(s);
  c->mutable_host_notification_commands()->add_data("non_existing_command");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A contact notified only on host recovery (not on down/unreachable) yields one
// warning.
TEST_F(Pb_Resolve, ContactWithOnlyHostRecoveryNotification) {
  State s = base_state();
  Contact* c = add_valid_contact(s);
  c->set_host_notification_options(action_hst_up);
  c->set_service_notification_options(action_svc_none);

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A contact notified only on service recovery (not on warning/critical) yields
// one warning.
TEST_F(Pb_Resolve, ContactWithOnlyServiceRecoveryNotification) {
  State s = base_state();
  Contact* c = add_valid_contact(s);
  c->set_host_notification_options(action_hst_none);
  c->set_service_notification_options(action_svc_ok);

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A contact whose name contains a character listed in illegal_object_chars is
// a single error.
TEST_F(Pb_Resolve, ContactNameWithIllegalChars) {
  State s = base_state();
  Contact* c = add_valid_contact(s);
  c->set_contact_name("adm!n");

  state_helper hlp(&s);
  // Set after construction: state_helper::_init() resets illegal_object_chars
  // (in production the parser fills it afterwards, like here).
  s.set_illegal_object_chars("!$");
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// The same contact name is fine when illegal_object_chars is empty (default):
// the check is disabled.
TEST_F(Pb_Resolve, ContactNameIllegalCharsDisabledWhenEmpty) {
  State s = base_state();
  Contact* c = add_valid_contact(s);
  c->set_contact_name("adm!n");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A contact group whose members all exist resolves without warning or error.
TEST_F(Pb_Resolve, ContactgroupValid) {
  State s = base_state();
  add_valid_contact(s);  // contact "admin"
  Contactgroup* cg = s.add_contactgroups();
  cg->set_contactgroup_name("cg1");
  cg->mutable_members()->add_data("admin");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A contact group referencing a non-existing contact is a single error.
TEST_F(Pb_Resolve, ContactgroupNonExistingMember) {
  State s = base_state();
  add_valid_contact(s);  // contact "admin"
  Contactgroup* cg = s.add_contactgroups();
  cg->set_contactgroup_name("cg1");
  cg->mutable_members()->add_data("ghost");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A contact group whose name contains a character listed in illegal_object_chars
// is a single error.
TEST_F(Pb_Resolve, ContactgroupNameWithIllegalChars) {
  State s = base_state();
  add_valid_contact(s);  // contact "admin"
  Contactgroup* cg = s.add_contactgroups();
  cg->set_contactgroup_name("cg!1");
  cg->mutable_members()->add_data("admin");

  state_helper hlp(&s);
  s.set_illegal_object_chars("!$");
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host group whose members are all defined resolves cleanly.
TEST_F(Pb_Resolve, HostgroupValid) {
  State s = base_state();
  add_host(s, "host_1");
  Hostgroup* hg = s.add_hostgroups();
  hg->set_hostgroup_name("hg1");
  hg->mutable_members()->add_data("host_1");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A host group referencing a non-existing host is a single error.
TEST_F(Pb_Resolve, HostgroupNonExistingMember) {
  State s = base_state();
  add_host(s, "host_1");
  Hostgroup* hg = s.add_hostgroups();
  hg->set_hostgroup_name("hg1");
  hg->mutable_members()->add_data("ghost");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host group whose name contains a character listed in illegal_object_chars
// is a single error.
TEST_F(Pb_Resolve, HostgroupNameWithIllegalChars) {
  State s = base_state();
  add_host(s, "host_1");
  Hostgroup* hg = s.add_hostgroups();
  hg->set_hostgroup_name("hg!1");
  hg->mutable_members()->add_data("host_1");

  state_helper hlp(&s);
  s.set_illegal_object_chars("!$");
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A service group whose members are all defined resolves cleanly.
TEST_F(Pb_Resolve, ServicegroupValid) {
  State s = base_state();
  add_host(s, "host_1");
  add_service(s, "host_1", "svc_1");
  Servicegroup* sg = s.add_servicegroups();
  sg->set_servicegroup_name("sg1");
  auto* m = sg->mutable_members()->add_data();
  m->set_first("host_1");
  m->set_second("svc_1");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A service group referencing a non-existing service is a single error.
TEST_F(Pb_Resolve, ServicegroupNonExistingMember) {
  State s = base_state();
  add_host(s, "host_1");
  add_service(s, "host_1", "svc_1");
  Servicegroup* sg = s.add_servicegroups();
  sg->set_servicegroup_name("sg1");
  auto* m = sg->mutable_members()->add_data();
  m->set_first("host_1");
  m->set_second("ghost_svc");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A service group whose name contains a character listed in
// illegal_object_chars is a single error.
TEST_F(Pb_Resolve, ServicegroupNameWithIllegalChars) {
  State s = base_state();
  add_host(s, "host_1");
  add_service(s, "host_1", "svc_1");
  Servicegroup* sg = s.add_servicegroups();
  sg->set_servicegroup_name("sg!1");
  auto* m = sg->mutable_members()->add_data();
  m->set_first("host_1");
  m->set_second("svc_1");

  state_helper hlp(&s);
  s.set_illegal_object_chars("!$");
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}
