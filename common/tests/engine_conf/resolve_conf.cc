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

#include <absl/algorithm/container.h>
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

  // Add a defined host to the State. A check period is set so the notifier
  // validation (now run on every host) does not raise the "no check time
  // period" warning. NOTE: a host with no service attached still raises the
  // "no services associated" warning in host_helper::resolve, so every test
  // that adds such a host must account for one extra config_warning.
  static void add_host(State& s, const std::string& name) {
    Host* h = s.add_hosts();
    h->set_host_name(name);
    h->set_check_period("24x7");
  }

  // Add a defined service to the State (with a check period, see add_host).
  static void add_service(State& s,
                          const std::string& host,
                          const std::string& description) {
    Service* svc = s.add_services();
    svc->set_host_name(host);
    svc->set_service_description(description);
    svc->set_check_period("24x7");
  }

  // Add a host dependency between two hosts. It uses the single-host form and a
  // known dependency type so that expand() keeps it as-is instead of
  // decomposing it (which would run resolve() several times).
  static Hostdependency* add_hostdependency(State& s,
                                            const std::string& dependent_host,
                                            const std::string& host) {
    Hostdependency* hd = s.add_hostdependencies();
    hd->set_dependency_type(DependencyKind::notification_dependency);
    hd->mutable_dependent_hosts()->add_data(dependent_host);
    hd->mutable_hosts()->add_data(host);
    return hd;
  }

  // Add a service dependency between two services. It keeps the default
  // (unknown) dependency type: servicedependency::expand rebuilds its whole
  // list from scratch and decomposes such a dependency into its notification
  // and execution variants, exactly like a parsed configuration. Each
  // validation issue is therefore reported once per variant (i.e. twice).
  static Servicedependency* add_servicedependency(State& s,
                                                  const std::string& dep_host,
                                                  const std::string& dep_svc,
                                                  const std::string& host,
                                                  const std::string& svc) {
    Servicedependency* sd = s.add_servicedependencies();
    sd->mutable_dependent_hosts()->add_data(dep_host);
    sd->mutable_dependent_service_description()->add_data(dep_svc);
    sd->mutable_hosts()->add_data(host);
    sd->mutable_service_description()->add_data(svc);
    return sd;
  }

  // Add a contact group (with no member) to the State.
  static Contactgroup* add_contactgroup(State& s, const std::string& name) {
    Contactgroup* cg = s.add_contactgroups();
    cg->set_contactgroup_name(name);
    return cg;
  }

  // Add a host escalation for a single host (expand keeps it as one entry).
  static Hostescalation* add_hostescalation(State& s, const std::string& host) {
    Hostescalation* he = s.add_hostescalations();
    he->mutable_hosts()->add_data(host);
    return he;
  }

  // Add a service escalation for a single (host, service) pair.
  static Serviceescalation* add_serviceescalation(State& s,
                                                  const std::string& host,
                                                  const std::string& svc) {
    Serviceescalation* se = s.add_serviceescalations();
    se->mutable_hosts()->add_data(host);
    se->mutable_service_description()->add_data(svc);
    return se;
  }

  // Add a host with a defined check period so the notifier-common validation
  // does not raise the "no check time period" warning. Built as raw protobuf,
  // notifications_enabled stays false, so no notification-period warning
  // either. As for add_host, a host with no service raises the "no services"
  // warning.
  static Host* add_notifier_host(State& s, const std::string& name) {
    Host* h = s.add_hosts();
    h->set_host_name(name);
    h->set_check_period("24x7");
    /* A real id: an anomaly detection carries the id of its host and Engine
     * refuses the pair when the two disagree. */
    h->set_host_id(s.hosts_size());
    return h;
  }

  // The id of an already added host, 0 when it is not defined.
  static uint64_t host_id_of(const State& s, const std::string& name) {
    auto it = absl::c_find_if(
        s.hosts(), [&name](const Host& h) { return h.host_name() == name; });
    return it == s.hosts().end() ? 0 : it->host_id();
  }

  // Add a service on a host, with a defined check period (see
  // add_notifier_host).
  static Service* add_notifier_service(State& s,
                                       const std::string& host,
                                       const std::string& desc) {
    Service* svc = s.add_services();
    svc->set_host_name(host);
    svc->set_service_description(desc);
    svc->set_check_period("24x7");
    // A real id: an anomaly detection designates its dependent service by id.
    svc->set_service_id(s.services_size());
    return svc;
  }

  /* Add an anomaly detection on a host, computed from the service
   * @a dependent_service_id of that same host. It has no check command/period
   * fields. The ids and the scheduling values are set because Engine refuses an
   * anomaly detection carrying a null one. */
  static Anomalydetection* add_anomalydetection(State& s,
                                                const std::string& host,
                                                const std::string& desc,
                                                uint64_t dependent_service_id) {
    Anomalydetection* ad = s.add_anomalydetections();
    ad->set_host_name(host);
    ad->set_service_description(desc);
    ad->set_host_id(host_id_of(s, host));
    ad->set_service_id(s.services_size() + s.anomalydetections_size());
    ad->set_internal_id(s.anomalydetections_size());
    ad->set_dependent_service_id(dependent_service_id);
    ad->set_max_check_attempts(3);
    ad->set_retry_interval(1);
    return ad;
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

// A contact group whose name contains a character listed in
// illegal_object_chars is a single error.
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

// A host group whose members are all defined resolves cleanly (the sole warning
// is the member host having no service attached).
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
  ASSERT_EQ(err.config_warnings, 1u);
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
  ASSERT_EQ(err.config_warnings, 1u);
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
  ASSERT_EQ(err.config_warnings, 1u);
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

// A host dependency between two defined hosts and a defined dependency period
// resolves without error (the two warnings are the hosts having no service).
TEST_F(Pb_Resolve, HostdependencyValid) {
  State s = base_state();
  add_host(s, "host_1");
  add_host(s, "host_2");
  Hostdependency* hd = add_hostdependency(s, "host_1", "host_2");
  hd->set_dependency_period("24x7");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 2u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A host dependency whose dependent host is not defined is a single error.
TEST_F(Pb_Resolve, HostdependencyNonExistingDependentHost) {
  State s = base_state();
  add_host(s, "host_2");
  add_hostdependency(s, "ghost", "host_2");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host dependency whose master host is not defined is a single error.
TEST_F(Pb_Resolve, HostdependencyNonExistingMasterHost) {
  State s = base_state();
  add_host(s, "host_1");
  add_hostdependency(s, "host_1", "ghost");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host dependency of a host on itself is a single error (circular).
TEST_F(Pb_Resolve, HostdependencyCircular) {
  State s = base_state();
  add_host(s, "host_1");
  add_hostdependency(s, "host_1", "host_1");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host dependency referencing a non-existing dependency period is a single
// error.
TEST_F(Pb_Resolve, HostdependencyNonExistingDependencyPeriod) {
  State s = base_state();
  add_host(s, "host_1");
  add_host(s, "host_2");
  Hostdependency* hd = add_hostdependency(s, "host_1", "host_2");
  hd->set_dependency_period("ghost_tp");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 2u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A service dependency between two defined services and a defined dependency
// period resolves without any warning or error (both expanded variants are
// valid).
TEST_F(Pb_Resolve, ServicedependencyValid) {
  State s = base_state();
  add_host(s, "host_1");
  add_host(s, "host_2");
  add_service(s, "host_1", "svc_1");
  add_service(s, "host_2", "svc_2");
  Servicedependency* sd =
      add_servicedependency(s, "host_1", "svc_1", "host_2", "svc_2");
  sd->set_dependency_period("24x7");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A service dependency whose dependent service is not defined is an error,
// reported once per expanded variant (notification + execution).
TEST_F(Pb_Resolve, ServicedependencyNonExistingDependentService) {
  State s = base_state();
  add_host(s, "host_1");
  add_host(s, "host_2");
  add_service(s, "host_2", "svc_2");
  add_servicedependency(s, "host_1", "ghost_svc", "host_2", "svc_2");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 2u);
}

// A service dependency whose master service is not defined is an error,
// reported once per expanded variant (notification + execution).
TEST_F(Pb_Resolve, ServicedependencyNonExistingMasterService) {
  State s = base_state();
  add_host(s, "host_1");
  add_host(s, "host_2");
  add_service(s, "host_1", "svc_1");
  add_servicedependency(s, "host_1", "svc_1", "host_2", "ghost_svc");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 2u);
}

// A service dependency of a service on itself is a circular error, reported
// once per expanded variant (notification + execution).
TEST_F(Pb_Resolve, ServicedependencyCircular) {
  State s = base_state();
  add_host(s, "host_1");
  add_service(s, "host_1", "svc_1");
  add_servicedependency(s, "host_1", "svc_1", "host_1", "svc_1");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 2u);
}

// A service dependency referencing a non-existing dependency period is an
// error, reported once per expanded variant (notification + execution).
TEST_F(Pb_Resolve, ServicedependencyNonExistingDependencyPeriod) {
  State s = base_state();
  add_host(s, "host_1");
  add_host(s, "host_2");
  add_service(s, "host_1", "svc_1");
  add_service(s, "host_2", "svc_2");
  Servicedependency* sd =
      add_servicedependency(s, "host_1", "svc_1", "host_2", "svc_2");
  sd->set_dependency_period("ghost_tp");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 2u);
}

// A host escalation on a defined host, with a defined contact group and a
// defined escalation period, resolves without error (the sole warning is the
// host having no service attached).
TEST_F(Pb_Resolve, HostescalationValid) {
  State s = base_state();
  add_host(s, "host_1");
  add_contactgroup(s, "cg1");
  Hostescalation* he = add_hostescalation(s, "host_1");
  he->mutable_contactgroups()->add_data("cg1");
  he->set_escalation_period("24x7");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A host escalation on a non-existing host is a single error.
TEST_F(Pb_Resolve, HostescalationNonExistingHost) {
  State s = base_state();
  add_hostescalation(s, "ghost");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host escalation referencing a non-existing contact group is a single error.
TEST_F(Pb_Resolve, HostescalationNonExistingContactgroup) {
  State s = base_state();
  add_host(s, "host_1");
  Hostescalation* he = add_hostescalation(s, "host_1");
  he->mutable_contactgroups()->add_data("ghost_cg");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host escalation referencing a non-existing escalation period is a single
// error.
TEST_F(Pb_Resolve, HostescalationNonExistingEscalationPeriod) {
  State s = base_state();
  add_host(s, "host_1");
  Hostescalation* he = add_hostescalation(s, "host_1");
  he->set_escalation_period("ghost_tp");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A service escalation on a defined service, with a defined contact group and a
// defined escalation period, resolves without any warning or error.
TEST_F(Pb_Resolve, ServiceescalationValid) {
  State s = base_state();
  add_host(s, "host_1");
  add_service(s, "host_1", "svc_1");
  add_contactgroup(s, "cg1");
  Serviceescalation* se = add_serviceescalation(s, "host_1", "svc_1");
  se->mutable_contactgroups()->add_data("cg1");
  se->set_escalation_period("24x7");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A service escalation on a non-existing service is a single error.
TEST_F(Pb_Resolve, ServiceescalationNonExistingService) {
  State s = base_state();
  add_host(s, "host_1");
  add_serviceescalation(s, "host_1", "ghost_svc");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A service escalation referencing a non-existing contact group is a single
// error.
TEST_F(Pb_Resolve, ServiceescalationNonExistingContactgroup) {
  State s = base_state();
  add_host(s, "host_1");
  add_service(s, "host_1", "svc_1");
  Serviceescalation* se = add_serviceescalation(s, "host_1", "svc_1");
  se->mutable_contactgroups()->add_data("ghost_cg");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A service escalation referencing a non-existing escalation period is a single
// error.
TEST_F(Pb_Resolve, ServiceescalationNonExistingEscalationPeriod) {
  State s = base_state();
  add_host(s, "host_1");
  add_service(s, "host_1", "svc_1");
  Serviceescalation* se = add_serviceescalation(s, "host_1", "svc_1");
  se->set_escalation_period("ghost_tp");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host with a defined check period and check command resolves without error
// (the sole warning is the host having no service attached).
TEST_F(Pb_Resolve, HostValid) {
  State s = base_state();
  Host* h = add_notifier_host(s, "host_1");
  h->set_check_command("cmd");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A host whose check command is not defined is a single error.
TEST_F(Pb_Resolve, HostNonExistingCheckCommand) {
  State s = base_state();
  Host* h = add_notifier_host(s, "host_1");
  h->set_check_command("ghost_cmd");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host referencing a non-existing parent host is a single error.
TEST_F(Pb_Resolve, HostNonExistingParent) {
  State s = base_state();
  Host* h = add_notifier_host(s, "host_1");
  h->mutable_parents()->add_data("ghost");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host referencing a non-existing contact group is a single error.
TEST_F(Pb_Resolve, HostNonExistingContactgroup) {
  State s = base_state();
  Host* h = add_notifier_host(s, "host_1");
  h->mutable_contactgroups()->add_data("ghost_cg");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 1u);
  ASSERT_EQ(err.config_errors, 1u);
}

// A host with no check time period gets a single warning (no error).
TEST_F(Pb_Resolve, HostNoCheckPeriodWarning) {
  State s = base_state();
  Host* h = s.add_hosts();
  h->set_host_name("host_1");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 2u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A service on a defined host, with a defined check period and check command,
// resolves without any warning or error.
TEST_F(Pb_Resolve, ServiceValid) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  Service* svc = add_notifier_service(s, "host_1", "svc_1");
  svc->set_check_command("cmd");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 0u);
}

// A service whose check command is not defined is a single error.
TEST_F(Pb_Resolve, ServiceNonExistingCheckCommand) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  Service* svc = add_notifier_service(s, "host_1", "svc_1");
  svc->set_check_command("ghost_cmd");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// An anomaly detection on a defined host resolves cleanly (it has no check
// command/period, so the notifier-common validation skips those checks).
TEST_F(Pb_Resolve, AnomalydetectionValid) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  Service* svc = add_notifier_service(s, "host_1", "svc_1");
  svc->set_check_command("cmd");
  add_anomalydetection(s, "host_1", "ad_1", svc->service_id());

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 0u);
}

// An anomaly detection referencing a non-existing contact group is a single
// error.
TEST_F(Pb_Resolve, AnomalydetectionNonExistingContactgroup) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  Service* svc = add_notifier_service(s, "host_1", "svc_1");
  svc->set_check_command("cmd");
  Anomalydetection* ad =
      add_anomalydetection(s, "host_1", "ad_1", svc->service_id());
  ad->mutable_contactgroups()->add_data("ghost_cg");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

/* The checks below each mirror a `return nullptr` of Engine's
 * add_anomalydetection(): the applier turns that null into a throw which, on a
 * first load, takes centengine down. They must be caught on the State. */

// An anomaly detection whose dependent service does not exist is one error.
TEST_F(Pb_Resolve, AnomalydetectionNonExistingDependentService) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  add_anomalydetection(s, "host_1", "ad_1", 42);

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// An anomaly detection comes from the database: a null service id is an error.
TEST_F(Pb_Resolve, AnomalydetectionNullServiceId) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  Service* svc = add_notifier_service(s, "host_1", "svc_1");
  svc->set_check_command("cmd");
  Anomalydetection* ad =
      add_anomalydetection(s, "host_1", "ad_1", svc->service_id());
  ad->set_service_id(0);

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// The internal id ties the anomaly detection to the database: it is mandatory.
TEST_F(Pb_Resolve, AnomalydetectionNullInternalId) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  Service* svc = add_notifier_service(s, "host_1", "svc_1");
  svc->set_check_command("cmd");
  Anomalydetection* ad =
      add_anomalydetection(s, "host_1", "ad_1", svc->service_id());
  ad->set_internal_id(0);

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

/* An anomaly detection carrying a host id that is not the one of the host it
 * names is an error. It is two of them: the dependent service is looked up with
 * that same wrong host id, so it cannot be found either. */
TEST_F(Pb_Resolve, AnomalydetectionHostIdMismatch) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  Service* svc = add_notifier_service(s, "host_1", "svc_1");
  svc->set_check_command("cmd");
  Anomalydetection* ad =
      add_anomalydetection(s, "host_1", "ad_1", svc->service_id());
  ad->set_host_id(ad->host_id() + 41);

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 2u);
}

// A null retry interval is a scheduling value Engine refuses.
TEST_F(Pb_Resolve, AnomalydetectionNullRetryInterval) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  Service* svc = add_notifier_service(s, "host_1", "svc_1");
  svc->set_check_command("cmd");
  Anomalydetection* ad =
      add_anomalydetection(s, "host_1", "ad_1", svc->service_id());
  ad->set_retry_interval(0);

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// Two anomaly detections sharing the same {host_id, service_id} is one error.
TEST_F(Pb_Resolve, AnomalydetectionDuplicateId) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  Service* svc = add_notifier_service(s, "host_1", "svc_1");
  svc->set_check_command("cmd");
  Anomalydetection* ad =
      add_anomalydetection(s, "host_1", "ad_1", svc->service_id());
  Anomalydetection* dup =
      add_anomalydetection(s, "host_1", "ad_2", svc->service_id());
  dup->set_service_id(ad->service_id());

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_warnings, 0u);
  ASSERT_EQ(err.config_errors, 1u);
}

// ---------------------------------------------------------------------------
// Circular-path detection, ported from the former Engine runtime
// pre_flight_circular_check into state_helper::resolve. The positive cases
// assert config_errors >= 1 (the exact number of loop members / dependency
// roots reported is an implementation detail); the negative case asserts 0 to
// prove there is no false positive.
// ---------------------------------------------------------------------------

// Three hosts whose parent relations form a cycle
// (host_1 -> host_2 -> host_3 -> host_1) are rejected.
TEST_F(Pb_Resolve, HostParentCircularPath) {
  State s = base_state();
  Host* h1 = add_notifier_host(s, "host_1");
  Host* h2 = add_notifier_host(s, "host_2");
  Host* h3 = add_notifier_host(s, "host_3");
  h1->mutable_parents()->add_data("host_2");
  h2->mutable_parents()->add_data("host_3");
  h3->mutable_parents()->add_data("host_1");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_GE(err.config_errors, 1u);
}

// Two hosts with mutual notification dependencies that inherit their parent
// form a circular notification dependency.
TEST_F(Pb_Resolve, HostNotificationDependencyCircular) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  add_notifier_host(s, "host_2");
  add_hostdependency(s, "host_1", "host_2")->set_inherits_parent(true);
  add_hostdependency(s, "host_2", "host_1")->set_inherits_parent(true);

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_GE(err.config_errors, 1u);
}

// The same mutual notification dependencies without inherits_parent do NOT
// form a cycle: notification dependencies only chain when they inherit.
TEST_F(Pb_Resolve, HostNotificationDependencyNotCircularWithoutInherit) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  add_notifier_host(s, "host_2");
  add_hostdependency(s, "host_1", "host_2");
  add_hostdependency(s, "host_2", "host_1");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_EQ(err.config_errors, 0u);
}

// Two services with mutual dependencies form a circular execution dependency:
// the default (unknown) type is decomposed by expand into notification and
// execution variants, and the execution variants chain unconditionally.
TEST_F(Pb_Resolve, ServiceExecutionDependencyCircular) {
  State s = base_state();
  add_notifier_host(s, "host_1");
  add_notifier_service(s, "host_1", "svc_1");
  add_notifier_service(s, "host_1", "svc_2");
  add_servicedependency(s, "host_1", "svc_1", "host_1", "svc_2");
  add_servicedependency(s, "host_1", "svc_2", "host_1", "svc_1");

  state_helper hlp(&s);
  error_cnt err;
  hlp.expand(err);
  hlp.resolve(err);
  ASSERT_GE(err.config_errors, 1u);
}
