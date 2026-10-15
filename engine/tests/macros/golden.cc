/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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
 */

// Golden test of the macro evaluation: every macro, in every form (context,
// named object, group), in several contexts, is evaluated on a fixed poller
// and compared to the reference file macros/golden.txt. It freezes the
// current behavior before the rewrite of the macros (see
// doc/engine-macros-en.md); a difference is either a regression or a wanted
// change, and then the reference file is regenerated with:
//
//   MACROS_GOLDEN_UPDATE=1 tests/ut_engine --gtest_filter='MacroGolden.*'

#include <absl/strings/escaping.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "../helper.hh"
#include "../test_engine.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostgroup.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/servicegroup.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/globals.hh"
#include "common/engine_conf/timeperiod_helper.hh"
#include "common/tests/timeperiods/utils.hh"
#include "engine/macros/descriptors.hh"
#include "engine/macros/process.hh"

using namespace com::centreon::engine;

namespace {

/* The evaluation contexts of a record. */
enum class ctx { none, host, service };

const char* ctx_name(ctx c) {
  switch (c) {
    case ctx::none:
      return "none";
    case ctx::host:
      return "host";
    default:
      return "svc";
  }
}

/* The fake clock of the test: Thursday 2016-11-24 08:00:00 (Europe/Paris). */
const time_t k_now = 1479970800;

void add_command(const std::string& name, const std::string& line) {
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name(name);
  cmd.set_command_line(line);
  configuration::applier::command aplyr;
  aplyr.add_object(cmd);
}

}  // namespace

class MacroGolden : public TestEngine {
 protected:
  std::unique_ptr<configuration::state_helper> _state_hlp;

 public:
  void SetUp() override {
    _state_hlp = init_config_state();
    set_time(k_now);
    _build_poller();
  }

  void TearDown() override { deinit_config_state(); }

 protected:
  /**
   * @brief Build the poller: hosts h1 (UP), h2 (DOWN, acknowledged) and h3
   * (UNREACHABLE, in downtime, child of h1), a service s on each host and a
   * service s2 on h1, contacts c1 (complete) and c2 (minimal), and the groups
   * hg, sg and cg.
   */
  void _build_poller() {
    auto& st = pb_indexed_config.mut_state();
    st.set_poller_name("poller-golden");
    st.set_admin_email("admin@example.com");
    st.set_admin_pager("+33611111111");
    st.set_illegal_output_chars("`~$&|'\"<>");
    (*st.mutable_users())["USER1"] = "/usr/lib/centreon/plugins";
    (*st.mutable_users())["USER3"] = "with `illegal` <chars> & 'quotes'";
    (*st.mutable_users())["CENTREONPLUGINS"] = "/usr/lib/centreon/plugins";

    add_command("notify", "true");
    add_command("check_ping",
                "$USER1$/check_ping -H $HOSTADDRESS$ -w $ARG1$ -c $ARG2$");
    add_command("check_http",
                "$USER1$/check_http -H $HOSTADDRESS$ -p $ARG1$ -u $ARG2$ "
                "-s '$_SERVICEBAR$' -t $_HOSTFOO$");
    {
      configuration::Timeperiod tp;
      configuration::timeperiod_helper tp_hlp(&tp);
      tp.set_timeperiod_name("24x7");
      tp.set_alias("24x7");
      for (const char* day : {"monday", "tuesday", "wednesday", "thursday",
                              "friday", "saturday", "sunday"})
        tp_hlp.hook(day, "00:00-24:00");
      configuration::applier::timeperiod aplyr;
      aplyr.add_object(tp);
    }
    {
      configuration::Timeperiod tp;
      configuration::timeperiod_helper tp_hlp(&tp);
      tp.set_timeperiod_name("workhours");
      tp.set_alias("workhours");
      for (const char* day :
           {"monday", "tuesday", "wednesday", "thursday", "friday"})
        tp_hlp.hook(day, "09:00-18:00");
      configuration::applier::timeperiod aplyr;
      aplyr.add_object(tp);
    }
    {
      configuration::Timeperiod tp;
      configuration::timeperiod_helper tp_hlp(&tp);
      tp.set_timeperiod_name("never");
      tp.set_alias("never");
      configuration::applier::timeperiod aplyr;
      aplyr.add_object(tp);
    }

    configuration::applier::contact ct_aply;
    std::vector<configuration::Contact> contacts(2);
    for (int i = 0; i < 2; ++i) {
      configuration::Contact& c = contacts[i];
      configuration::contact_helper c_hlp(&c);
      c.set_contact_name("c" + std::to_string(i + 1));
      c.set_host_notification_period("24x7");
      c.set_service_notification_period("24x7");
      fill_string_group(c.mutable_host_notification_commands(), "notify");
      fill_string_group(c.mutable_service_notification_commands(), "notify");
      c_hlp.hook("host_notification_options", "d,u,r,f,s");
      c_hlp.hook("service_notification_options", "a");
      c.set_host_notifications_enabled(true);
      c.set_service_notifications_enabled(true);
      c_hlp.insert_customvariable("_BAZ", "baz-c" + std::to_string(i + 1));
    }
    contacts[0].set_alias("Contact One");
    contacts[0].set_email("c1@example.com");
    contacts[0].set_pager("+33600000001");
    contacts[0].add_address("address-one");
    contacts[0].add_address("address-two");
    contacts[0].add_address("address-three");
    contacts[0].set_timezone(":America/New_York");
    for (auto& c : contacts)
      ct_aply.add_object(c);

    configuration::applier::contactgroup cg_aply;
    configuration::Contactgroup cg;
    configuration::contactgroup_helper cg_hlp(&cg);
    cg.set_contactgroup_name("cg");
    cg.set_alias("Contact group");
    fill_string_group(cg.mutable_members(), "c1,c2");
    cg_aply.add_object(cg);

    configuration::applier::host hst_aply;
    std::vector<configuration::Host> hosts(3);
    for (int i = 0; i < 3; ++i) {
      configuration::Host& h = hosts[i];
      configuration::host_helper h_hlp(&h);
      std::string name = "h" + std::to_string(i + 1);
      h.set_host_name(name);
      h.set_host_id(i + 1);
      h.set_alias("Host " + std::to_string(i + 1));
      h.set_address("10.0.0." + std::to_string(i + 1));
      h.set_check_command("check_ping!200,20%!500,60%");
      h.set_check_period("24x7");
      h.set_notification_period("24x7");
      fill_string_group(h.mutable_contacts(), i == 0 ? "c1" : "c2");
      h_hlp.insert_customvariable("_FOO", "foo-" + name);
    }
    hosts[0].set_display_name("Host One");
    hosts[0].set_notes("notes of h1");
    hosts[0].set_notes_url("http://example.com/notes?host=$HOSTNAME$");
    hosts[0].set_action_url("http://example.com/action?host=$HOSTNAME$");
    hosts[0].set_icon_image("h1.png");
    hosts[0].set_icon_image_alt("icon of h1");
    hosts[0].set_timezone(":Asia/Tokyo");
    fill_string_group(hosts[2].mutable_parents(), "h1");
    for (auto& h : hosts)
      hst_aply.add_object(h);

    configuration::applier::hostgroup hg_aply;
    configuration::Hostgroup hg;
    configuration::hostgroup_helper hg_hlp(&hg);
    hg.set_hostgroup_name("hg");
    hg.set_alias("Host group");
    hg.set_notes("notes of hg & <co>");
    hg.set_notes_url("http://example.com/hg-notes?a=1&b=2");
    hg.set_action_url("http://example.com/hg-action?a=1&b=2");
    hg_hlp.hook("members", "h1,h2,h3");
    hg_aply.add_object(hg);

    configuration::applier::service svc_aply;
    const std::pair<const char*, const char*> svc_keys[] = {
        {"h1", "s"}, {"h1", "s2"}, {"h2", "s"}, {"h3", "s"}};
    std::vector<configuration::Service> services(std::size(svc_keys));
    for (size_t i = 0; i < services.size(); ++i) {
      configuration::Service& s = services[i];
      configuration::service_helper s_hlp(&s);
      auto [hname, desc] = svc_keys[i];
      s.set_host_name(hname);
      s.set_host_id(hname[1] - '0');
      s.set_service_description(desc);
      s.set_service_id(i + 1);
      s.set_check_command("check_http!80!/health");
      s.set_check_period("24x7");
      s.set_notification_period("24x7");
      fill_string_group(s.mutable_contacts(), i < 2 ? "c1" : "c2");
      s_hlp.insert_customvariable("_BAR", absl::StrCat("bar-", hname, desc));
    }
    services[0].set_display_name("Service One");
    services[0].set_notes("notes of h1/s");
    services[0].set_notes_url("http://example.com/notes?svc=$SERVICEDESC$");
    services[0].set_action_url("http://example.com/action?svc=$SERVICEDESC$");
    services[0].set_icon_image("s.png");
    services[0].set_icon_image_alt("icon of s");
    services[0].set_timezone(":Europe/London");
    for (auto& s : services)
      svc_aply.add_object(s);

    configuration::applier::servicegroup sg_aply;
    configuration::Servicegroup sg;
    configuration::servicegroup_helper sg_hlp(&sg);
    sg.set_servicegroup_name("sg");
    sg.set_alias("Service group");
    sg.set_notes("notes of sg & <co>");
    sg.set_notes_url("http://example.com/sg-notes?a=1&b=2");
    sg.set_action_url("http://example.com/sg-action?a=1&b=2");
    fill_pair_string_group(sg.mutable_members(), "h1,s,h2,s,h3,s");
    sg_aply.add_object(sg);
    /* A group of one service, for the $X:sg3$ form: with several members,
     * the values would be glued in no particular order. */
    configuration::Servicegroup sg3;
    configuration::servicegroup_helper sg3_hlp(&sg3);
    sg3.set_servicegroup_name("sg3");
    fill_pair_string_group(sg3.mutable_members(), "h3,s");
    sg_aply.add_object(sg3);

    configuration::error_cnt err;
    _state_hlp->expand(err);
    for (auto& c : contacts)
      ct_aply.resolve_object(c);
    cg_aply.resolve_object(cg);
    for (auto& h : hosts)
      hst_aply.resolve_object(h);
    hg_aply.resolve_object(hg);
    for (auto& s : services)
      svc_aply.resolve_object(s);
    sg_aply.resolve_object(sg);
    sg_aply.resolve_object(sg3);

    _set_states();
  }

  /* Fixed states, so that every state macro has a meaningful value. */
  void _set_states() {
    auto set_checkable = [](checkable* c, int k) {
      c->set_has_been_checked(true);
      c->set_last_check(k_now - 60 - k);
      c->set_last_state_change(k_now - 3725 - k);
      c->set_last_hard_state_change(k_now - 90061 - k);
      c->set_latency(0.25 + k);
      c->set_execution_time(1.5 + k);
      c->set_percent_state_change(12.5 + k);
      c->set_current_attempt(1 + k % 3);
      c->set_max_attempts(3);
      c->set_current_event_id(100 + k);
      c->set_last_event_id(90 + k);
      c->set_current_problem_id(k ? 200 + k : 0);
      c->set_last_problem_id(190 + k);
    };

    const std::tuple<const char*, host::host_state, AckType, int> hst[] = {
        {"h1", host::state_up, AckType::NONE, 0},
        {"h2", host::state_down, AckType::NORMAL, 0},
        {"h3", host::state_unreachable, AckType::NONE, 1},
    };
    int k = 0;
    for (auto& [name, state, ack, dt] : hst) {
      host* h = host::hosts.find(name)->second.get();
      set_checkable(h, k++);
      h->set_current_state(state);
      h->set_last_state(state);
      h->set_last_hard_state(state);
      h->set_state_type(checkable::hard);
      h->set_acknowledgement(ack);
      h->set_scheduled_downtime_depth(dt);
      h->set_plugin_output(absl::StrCat("PING output of ", name));
      h->set_perf_data("rta=0.5ms;200;500;0 pl=0%;20;60;0;100");
      h->set_notification_number(k);
    }

    const std::tuple<const char*, const char*, service::service_state, AckType>
        svc[] = {
            {"h1", "s", service::state_critical, AckType::NONE},
            {"h1", "s2", service::state_ok, AckType::NONE},
            {"h2", "s", service::state_warning, AckType::STICKY},
            {"h3", "s", service::state_unknown, AckType::NONE},
        };
    for (auto& [hname, desc, state, ack] : svc) {
      service* s = service::services.find({hname, desc})->second.get();
      set_checkable(s, k++);
      s->set_current_state(state);
      s->set_last_state(state);
      s->set_last_hard_state(state);
      s->set_state_type(checkable::hard);
      s->set_acknowledgement(ack);
      s->set_plugin_output(
          "CRITICAL - `rm -rf` $HOME & 'quoted' \"dquoted\" <tag> | pipe");
      s->set_long_plugin_output("line 1\nline 2 with $dollar\nline 3");
      s->set_perf_data("time=10.000000s;5.0;10.0;0.0 size=0B;;;0");
      s->set_notification_number(k);
    }
  }

  /**
   * @brief Fill the macros with the context: nothing, host h1 with
   * contact c1 (host notification), or h1, its service s and c1 (service
   * notification). The notification macros and the arguments are set as
   * notification_execution.cc and macro_context::set_arguments() do.
   */
  void _set_context(macro_context& mac, ctx c) {
    if (c == ctx::none)
      return;
    mac.set_host(host::hosts.find("h1")->second.get());
    if (c == ctx::service)
      mac.set_service(service::services.find({"h1", "s"})->second.get());
    mac.set_contact(contact::contacts.find("c1")->second.get());
    notification_values& notification = mac.notification.emplace();
    notification.type = com::centreon::common::notifications::reason_normal;
    notification.number = 3;
    notification.is_escalated = false;
    notification.recipients = "c1";
    notification.author = "ad`min";
    notification.author_name = "Admin|istrator";
    notification.author_alias = "admin <alias>";
    notification.comment = "it's a \"comment\" & $more";
    mac.argv[0] = "80";
    mac.argv[1] = "/health";
    mac.argv[2] = "arg with $ and `backquote`";
  }

  /**
   * @brief Evaluate a line and format the record of the reference file:
   * "<context> <options> <line> => <output>", with the line and the output
   * C-escaped.
   *
   * The members of a group come in no particular order (hash maps), so the
   * output of a group form ('#' separator) or of a *MEMBERS macro (',') is
   * normalized: its parts are sorted. The empty values are skipped, so an
   * empty part is a regression.
   */
  std::string _record(ctx c,
                      int options,
                      const std::string& line,
                      bool group = false) {
    macro_context mac;
    _set_context(mac, c);
    std::string out = macros::process_macros(mac, line, options);
    if (group || line.find("MEMBERS") != std::string::npos) {
      std::vector<std::string_view> parts =
          absl::StrSplit(out, absl::ByAnyChar("#,"));
      std::sort(parts.begin(), parts.end());
      out = absl::StrJoin(parts, "#");
    }
    return absl::StrCat(ctx_name(c), " ", options, " ", absl::CEscape(line),
                        " => ", absl::CEscape(out));
  }

  /* The flow of a check: the context, the arguments, the command line. */
  std::vector<std::string> _check_records() {
    std::vector<std::string> retval;
    macro_context mac;
    host* h = host::hosts.find("h1")->second.get();
    retval.push_back("check h1 => " +
                     absl::CEscape(h->get_check_command_line(mac)));
    mac.reset();
    service* s = service::services.find({"h1", "s"})->second.get();
    retval.push_back("check h1/s => " +
                     absl::CEscape(s->get_check_command_line(mac)));
    return retval;
  }

  /* All the records, in a fixed order. */
  std::vector<std::string> _records() {
    std::vector<std::string> retval;
    const ctx contexts[] = {ctx::none, ctx::host, ctx::service};

    /* Every macro name in every form; most forms make no sense for most
     * macros, their current result is frozen too. */
    std::vector<std::string> names;
    for (auto& desc : com::centreon::engine::macros::all_descriptors())
      names.emplace_back(desc.name);
    for (const char* n : {"_HOSTFOO",
                          "_HOSTfoo",
                          "_HOSTNOPE",
                          "_SERVICEBAR",
                          "_CONTACTBAZ",
                          "ARG1",
                          "ARG3",
                          "ARG4",
                          "ARG32",
                          "ARG33",
                          "USER1",
                          "USER2",
                          "USER3",
                          "USER256",
                          "USER257",
                          "CENTREONPLUGINS",
                          "CONTACTADDRESS0",
                          "CONTACTADDRESS1",
                          "CONTACTADDRESS3",
                          "CONTACTADDRESS6",
                          "CONTACTADDRESS7",
                          "NOSUCHMACRO",
                          "hostname"})
      names.emplace_back(n);

    const std::pair<const char*, bool> forms[] = {
        {"", false},     {":h2", false},      {":h2:s", false},
        {"::s", false},  {":h2:nope", false}, {":nope", false},
        {":hg:#", true}, {":sg:#", true},     {":c2", false},
        {":cg:#", true}, {":hg", false},      {"::", false},
        {":sg3", false},
    };
    for (auto& n : names)
      for (auto& [suffix, group] : forms)
        for (ctx c : contexts)
          retval.push_back(
              _record(c, 0, absl::StrCat("$", n, suffix, "$"), group));

    /* Parsing of the line itself. */
    const char* lines[] = {"no macro in this line",
                           "$$",
                           "$",
                           "a$b",
                           "$HOSTNAME",
                           "HOSTNAME$",
                           "$$HOSTNAME$$",
                           "$HOSTNAME$$SERVICEDESC$",
                           "[$HOSTNAME$] [$SERVICEDESC$] [$CONTACTNAME$]",
                           "$ HOSTNAME$",
                           "$HOSTNAME $",
                           "$:h2$",
                           "$$$HOSTNAME$$$",
                           "$ARG3$ $USER3$"};
    for (const char* l : lines)
      for (ctx c : contexts)
        retval.push_back(_record(c, 0, l));

    /* The cleaning options on the values with illegal characters. */
    const char* cleaned[] = {"$SERVICEOUTPUT$",
                             "$LONGSERVICEOUTPUT$",
                             "$SERVICEPERFDATA$",
                             "$HOSTOUTPUT$",
                             "$USER3$",
                             "$ARG3$",
                             "$_SERVICEBAR$",
                             "$HOSTNOTESURL$",
                             "$SERVICEACTIONURL$",
                             "$SERVICEOUTPUT:h2:s$"};
    for (const char* l : cleaned)
      retval.push_back(_record(ctx::service, STRIP_ILLEGAL_MACRO_CHARS, l));

    /* The timeperiod macros on a period, at now (Thursday 08:00) or at a
     * given timestamp: 07:00, 10:00, 20:00 on Thursday and 10:00 on
     * Saturday. */
    const std::string timestamps[] = {
        "", absl::StrCat(":", k_now - 3600), absl::StrCat(":", k_now + 7200),
        absl::StrCat(":", k_now + 12 * 3600),
        absl::StrCat(":", k_now + 2 * 86400 + 7200)};
    for (const char* n : {"ISVALIDTIME", "NEXTVALIDTIME"})
      for (const char* tp : {"24x7", "workhours", "never"})
        for (auto& ts : timestamps)
          retval.push_back(
              _record(ctx::none, 0, absl::StrCat("$", n, ":", tp, ts, "$")));

    for (auto& r : _check_records())
      retval.push_back(std::move(r));
    return retval;
  }
};

TEST_F(MacroGolden, AllMacros) {
  const std::string path = ENGINE_MACROS_GOLDEN;
  std::vector<std::string> actual = _records();

  if (std::getenv("MACROS_GOLDEN_UPDATE")) {
    std::ofstream f(path);
    for (auto& r : actual)
      f << r << '\n';
    ASSERT_TRUE(f.good()) << "cannot write " << path;
    GTEST_SKIP() << actual.size() << " records written to " << path;
  }

  std::ifstream f(path);
  ASSERT_TRUE(f.is_open()) << "cannot read " << path;
  std::vector<std::string> expected;
  for (std::string line; std::getline(f, line);)
    expected.push_back(std::move(line));

  /* A record per failure, so that a regression is readable. */
  int diffs = 0;
  for (size_t i = 0; i < std::min(expected.size(), actual.size()); ++i)
    if (expected[i] != actual[i] && ++diffs <= 50)
      ADD_FAILURE() << "record " << i + 1 << "\n  expected: " << expected[i]
                    << "\n  actual:   " << actual[i];
  ASSERT_EQ(diffs, 0) << diffs << " records differ";
  ASSERT_EQ(expected.size(), actual.size());
}
