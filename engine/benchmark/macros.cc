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

// Micro-benchmarks of the current macro evaluation of Engine, one family at a
// time (see doc/engine-macros-en.md):
//   - BM_MacroX/<NAME>     : every standard macro ($HOSTNAME$, $SERVICESTATE$,
//                            ...), evaluated on the context objects. Cached
//                            families ($TOTAL*$) are measured once cached.
//   - BM_Other/<label>     : the families outside the macro_x table (ARGn,
//                            USERn, user macros, custom variables,
//                            CONTACTADDRESSn, unknown macro, no macro).
//   - BM_OnDemand/<label>  : macros on a named object ($HOSTSTATE:h1$...).
//   - BM_Group/<label>/N   : concatenation over a group of N members.
//   - BM_Summary*          : the $TOTAL*$ macros when they are computed, i.e.
//                            the walk through every host and service.
//   - BM_Cleaning/<opt>    : an output to clean, with or without the options.
//   - BM_*CheckCommand, BM_NotificationLine : what Engine really does for a
//                            check or a notification, context setup included.
//
// The poller size is given by the environment:
//   MACROS_BENCH_HOSTS (default 1000) and MACROS_BENCH_SERVICES_PER_HOST
//   (default 10). The $TOTAL*$ macros depend on it, the others do not.

#include <benchmark/benchmark.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "com/centreon/common/pool.hh"
#include "com/centreon/engine/commands/command.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/hostgroup.hh"
#include "com/centreon/engine/configuration/applier/macros.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/applier/servicegroup.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/descriptors.hh"
#include "common/engine_conf/timeperiod_helper.hh"
#include "helper.hh"

using namespace com::centreon::engine;

std::shared_ptr<asio::io_context> g_io_context(
    std::make_shared<asio::io_context>());

namespace {

/* Group sizes measured by BM_Group. */
constexpr int k_group_sizes[] = {10, 100, 1000};
/* Number of contacts, the contact groups cannot be larger. */
constexpr int k_contacts = 100;

int env_int(const char* name, int dflt) {
  const char* v = std::getenv(name);
  return v ? std::atoi(v) : dflt;
}

std::string host_name(int i) {
  return "h" + std::to_string(i);
}
std::string service_desc(int j) {
  return "s" + std::to_string(j);
}
std::string contact_name(int i) {
  return "c" + std::to_string(i);
}

/* The poller built for the benchmarks. */
struct poller {
  int hosts;
  int services_per_host;
  host* hst = nullptr;
  service* svc = nullptr;
  contact* cntct = nullptr;
};

poller p;

void add_command(const std::string& name, const std::string& line) {
  configuration::Command cmd;
  configuration::command_helper cmd_hlp(&cmd);
  cmd.set_command_name(name);
  cmd.set_command_line(line);
  configuration::applier::command aplyr;
  aplyr.add_object(cmd);
}

/**
 * @brief Build the poller: commands, a 24x7 timeperiod, k_contacts contacts,
 * p.hosts hosts with p.services_per_host services each, and host, service and
 * contact groups of the sizes in k_group_sizes. The objects are created and
 * resolved by the appliers, as in the unit tests.
 *
 * @param state_hlp The helper on the configuration state.
 */
void build_poller(configuration::state_helper& state_hlp) {
  auto& st = pb_indexed_config.mut_state();
  st.set_poller_name("poller-bench");
  st.set_admin_email("admin@example.com");
  st.set_illegal_output_chars("`~$&|'\"<>");
  (*st.mutable_users())["USER1"] = "/usr/lib/centreon/plugins";
  (*st.mutable_users())["CENTREONPLUGINS"] = "/usr/lib/centreon/plugins";

  add_command("notify", "true");
  add_command("check_ping",
              "$USER1$/check_ping -H $HOSTADDRESS$ -w $ARG1$ -c $ARG2$");
  add_command("check_http",
              "$USER1$/check_http -H $HOSTADDRESS$ -p $ARG1$ -u $ARG2$");
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

  std::vector<configuration::Contact> contacts(k_contacts);
  configuration::applier::contact ct_aply;
  for (int i = 0; i < k_contacts; ++i) {
    configuration::Contact& c = contacts[i];
    configuration::contact_helper c_hlp(&c);
    c.set_contact_name(contact_name(i));
    c.set_alias("Contact " + std::to_string(i));
    c.set_email(contact_name(i) + "@example.com");
    c.set_pager("+33600000000");
    c.add_address("address-one");
    c.set_host_notification_period("24x7");
    c.set_service_notification_period("24x7");
    fill_string_group(c.mutable_host_notification_commands(), "notify");
    fill_string_group(c.mutable_service_notification_commands(), "notify");
    c_hlp.hook("host_notification_options", "d,r,f,s");
    c_hlp.hook("service_notification_options", "a");
    c.set_host_notifications_enabled(true);
    c.set_service_notifications_enabled(true);
    c_hlp.insert_customvariable("_BAZ", "contact-custom");
    ct_aply.add_object(c);
  }

  std::vector<configuration::Contactgroup> cgs;
  for (int size : k_group_sizes) {
    if (size > k_contacts)
      continue;
    configuration::Contactgroup& cg = cgs.emplace_back();
    configuration::contactgroup_helper cg_hlp(&cg);
    cg.set_contactgroup_name("cg_" + std::to_string(size));
    cg.set_alias("cg_" + std::to_string(size));
    std::string members;
    for (int i = 0; i < size; ++i)
      absl::StrAppend(&members, i ? "," : "", contact_name(i));
    fill_string_group(cg.mutable_members(), members);
  }
  configuration::applier::contactgroup cg_aply;
  for (auto& cg : cgs)
    cg_aply.add_object(cg);

  std::vector<configuration::Host> hosts(p.hosts);
  configuration::applier::host hst_aply;
  for (int i = 0; i < p.hosts; ++i) {
    configuration::Host& h = hosts[i];
    configuration::host_helper h_hlp(&h);
    h.set_host_name(host_name(i));
    h.set_host_id(i + 1);
    h.set_alias("Host " + std::to_string(i));
    h.set_address("10.0." + std::to_string(i / 256) + "." +
                  std::to_string(i % 256));
    h.set_check_command("check_ping!200,20%!500,60%");
    fill_string_group(h.mutable_contacts(), contact_name(i % k_contacts));
    h_hlp.insert_customvariable("_FOO", "host-custom");
    hst_aply.add_object(h);
  }

  std::vector<configuration::Hostgroup> hgs;
  for (int size : k_group_sizes) {
    if (size > p.hosts)
      continue;
    configuration::Hostgroup& hg = hgs.emplace_back();
    configuration::hostgroup_helper hg_hlp(&hg);
    hg.set_hostgroup_name("hg_" + std::to_string(size));
    hg.set_alias("hg_" + std::to_string(size));
    std::string members;
    for (int i = 0; i < size; ++i)
      absl::StrAppend(&members, i ? "," : "", host_name(i));
    hg_hlp.hook("members", members);
  }
  configuration::applier::hostgroup hg_aply;
  for (auto& hg : hgs)
    hg_aply.add_object(hg);

  std::vector<configuration::Service> services(p.hosts * p.services_per_host);
  configuration::applier::service svc_aply;
  for (int i = 0; i < p.hosts; ++i)
    for (int j = 0; j < p.services_per_host; ++j) {
      configuration::Service& s = services[i * p.services_per_host + j];
      configuration::service_helper s_hlp(&s);
      s.set_host_name(host_name(i));
      s.set_host_id(i + 1);
      s.set_service_description(service_desc(j));
      s.set_service_id(j + 1);
      s.set_check_command("check_http!80!/health");
      fill_string_group(s.mutable_contacts(), contact_name(i % k_contacts));
      s_hlp.insert_customvariable("_BAR", "service-custom");
      svc_aply.add_object(s);
    }

  std::vector<configuration::Servicegroup> sgs;
  for (int size : k_group_sizes) {
    if (size > p.hosts * p.services_per_host)
      continue;
    configuration::Servicegroup& sg = sgs.emplace_back();
    configuration::servicegroup_helper sg_hlp(&sg);
    sg.set_servicegroup_name("sg_" + std::to_string(size));
    sg.set_alias("sg_" + std::to_string(size));
    std::string members;
    for (int k = 0; k < size; ++k)
      absl::StrAppend(&members, k ? "," : "",
                      host_name(k / p.services_per_host), ",",
                      service_desc(k % p.services_per_host));
    fill_pair_string_group(sg.mutable_members(), members);
  }
  configuration::applier::servicegroup sg_aply;
  for (auto& sg : sgs)
    sg_aply.add_object(sg);

  configuration::error_cnt err;
  state_hlp.expand(err);
  for (auto& c : contacts)
    ct_aply.resolve_object(c);
  for (auto& cg : cgs)
    cg_aply.resolve_object(cg);
  for (auto& h : hosts)
    hst_aply.resolve_object(h);
  for (auto& hg : hgs)
    hg_aply.resolve_object(hg);
  for (auto& s : services)
    svc_aply.resolve_object(s);
  for (auto& sg : sgs)
    sg_aply.resolve_object(sg);

  configuration::applier::macros::instance().apply(st);

  /* Some states, so that the $TOTAL*$ macros have something to count. */
  for (auto& [name, h] : host::hosts) {
    h->set_current_state(host::state_up);
    h->set_has_been_checked(true);
  }
  int k = 0;
  for (auto& [key, s] : service::services) {
    s->set_current_state(static_cast<service::service_state>(k++ % 4));
    s->set_has_been_checked(true);
    s->set_plugin_output("CRITICAL - Socket timeout after 10 seconds");
    s->set_long_plugin_output("line 1\nline 2\nline 3");
    s->set_perf_data("time=10.000000s;5.0;10.0;0.0 size=0B;;;0");
  }

  p.hst = host::hosts.find(host_name(0))->second.get();
  p.svc = service::services.find({host_name(0), service_desc(0)})->second.get();
  p.cntct = contact::contacts.find(contact_name(0))->second.get();
}

/* The macros context of the benchmarks. */
macro_context context;

/**
 * @brief Fill the macros context with the context of a service notification
 * to contact c0, as notification_execution.cc does.
 */
macro_context* set_context() {
  macro_context* mac = &context;
  mac->reset();
  mac->set_host(p.hst);
  mac->set_service(p.svc);
  mac->set_contact(p.cntct);
  notification_values& notification = mac->notification.emplace();
  notification.type = com::centreon::common::notifications::reason_normal;
  notification.number = 1;
  notification.is_escalated = false;
  notification.recipients = "c0";
  notification.author = "admin";
  notification.comment = "a comment";
  mac->argv[0] = "80";
  mac->argv[1] = "/health";
  return mac;
}

/* Evaluate one line in the notification context. */
void eval_line(benchmark::State& state, const std::string& line, int options) {
  macro_context* mac = set_context();
  std::string out;
  for (auto _ : state) {
    process_macros_r(mac, line, out, options);
    benchmark::DoNotOptimize(out);
  }
  state.SetLabel(out.size() > 40 ? out.substr(0, 40) + "..." : out);
}

void BM_MacroX(benchmark::State& state, std::string line) {
  eval_line(state, line, 0);
}

void BM_Other(benchmark::State& state, std::string line) {
  eval_line(state, line, 0);
}

void BM_OnDemand(benchmark::State& state, std::string line) {
  eval_line(state, line, 0);
}

/* line contains "{}" in place of the group size. */
void BM_Group(benchmark::State& state, std::string line) {
  std::string size = std::to_string(state.range(0));
  size_t pos = line.find("{}");
  line.replace(pos, 2, size);
  eval_line(state, line, 0);
}

/* The 16 $TOTAL*$ macros are computed at once on the first one used, then
 * cached: here the cache is emptied at each iteration, as for each contact of
 * a notification. state.range(0) tells whether a contact filters the totals. */
void BM_SummaryComputed(benchmark::State& state) {
  macro_context* mac = set_context();
  if (!state.range(0))
    mac->contact_ptr = nullptr;
  std::string out;
  for (auto _ : state) {
    mac->summary.reset();
    process_macros_r(mac, "$TOTALSERVICESCRITICALUNHANDLED$", out, 0);
    benchmark::DoNotOptimize(out);
  }
  state.SetLabel(state.range(0) ? "with contact" : "without contact");
}

/* $TOTALHOSTSERVICES*$: walk through the services of the host, cached until
 * the next set_host(). */
void BM_HostTotalsComputed(benchmark::State& state) {
  macro_context* mac = set_context();
  std::string out;
  for (auto _ : state) {
    mac->set_host(p.hst);
    process_macros_r(mac, "$TOTALHOSTSERVICESCRITICAL$", out, 0);
    benchmark::DoNotOptimize(out);
  }
}

void BM_Cleaning(benchmark::State& state) {
  eval_line(state, "$SERVICEOUTPUT$ $SERVICEPERFDATA$",
            state.range(0) ? STRIP_ILLEGAL_MACRO_CHARS : 0);
}

/* What a service check does: its own context, arguments, command line. */
void BM_ServiceCheckCommand(benchmark::State& state) {
  std::string out;
  for (auto _ : state) {
    macro_context local_macros;
    out = p.svc->get_check_command_line(&local_macros);
    benchmark::DoNotOptimize(out);
  }
  state.SetLabel(out);
}

/* The same for a host check (checker.cc / host.cc). */
void BM_HostCheckCommand(benchmark::State& state) {
  std::string out;
  std::string tmp;
  for (auto _ : state) {
    macro_context local_macros;
    macro_context* mac(&local_macros);
    mac->set_host(p.hst);
    get_raw_command_line_r(mac, p.hst->get_check_command_ptr(),
                           p.hst->check_command(), tmp, 0);
    out = p.hst->get_check_command_ptr()->process_cmd(mac);
    benchmark::DoNotOptimize(out);
  }
  state.SetLabel(out);
}

/* The default Centreon "service-notify-by-email" line, evaluated for one
 * contact as service::notify_contact() does. */
void BM_NotificationLine(benchmark::State& state) {
  static const std::string line =
      "/usr/bin/printf \"%b\" \"***** Centreon *****\\n\\nNotification Type: "
      "$NOTIFICATIONTYPE$\\n\\nService: $SERVICEDESC$\\nHost: "
      "$HOSTALIAS$\\nAddress: $HOSTADDRESS$\\nState: "
      "$SERVICESTATE$\\n\\nDate/Time: $LONGDATETIME$\\n\\nAdditional "
      "Info:\\n\\n$SERVICEOUTPUT$\\n$LONGSERVICEOUTPUT$\" | /bin/mail -s \"** "
      "$NOTIFICATIONTYPE$ Service Alert: $HOSTNAME$/$SERVICEDESC$ is "
      "$SERVICESTATE$ **\" $CONTACTEMAIL$";
  macro_context* mac = set_context();
  std::string out;
  for (auto _ : state) {
    mac->set_contact(p.cntct);
    process_macros_r(mac, line, out, STRIP_ILLEGAL_MACRO_CHARS);
    benchmark::DoNotOptimize(out);
  }
}

void register_benchmarks() {
  for (auto& desc : com::centreon::engine::macros::all_descriptors()) {
    std::string name(desc.name);
    benchmark::RegisterBenchmark(
        ("BM_MacroX/" + std::to_string(desc.id) + "/" + name).c_str(),
        BM_MacroX, "$" + name + "$");
  }

  const std::pair<const char*, const char*> others[] = {
      {"ARG1", "$ARG1$"},
      {"USER1", "$USER1$"},
      {"user_macro", "$CENTREONPLUGINS$"},
      {"custom_host", "$_HOSTFOO$"},
      {"custom_service", "$_SERVICEBAR$"},
      {"custom_contact", "$_CONTACTBAZ$"},
      {"CONTACTADDRESS1", "$CONTACTADDRESS1$"},
      {"unknown", "$NOSUCHMACRO$"},
      {"dollar_escape", "$$"},
      {"no_macro", "no macro in this line"},
  };
  for (auto& [label, line] : others)
    benchmark::RegisterBenchmark((std::string("BM_Other/") + label).c_str(),
                                 BM_Other, std::string(line));

  const std::pair<const char*, const char*> on_demand[] = {
      {"HOSTSTATE", "$HOSTSTATE:h1$"},
      {"SERVICESTATE", "$SERVICESTATE:h1:s1$"},
      {"SERVICESTATE_current_host", "$SERVICESTATE::s1$"},
      {"CONTACTEMAIL", "$CONTACTEMAIL:c1$"},
      {"custom_host", "$_HOSTFOO:h1$"},
  };
  for (auto& [label, line] : on_demand)
    benchmark::RegisterBenchmark((std::string("BM_OnDemand/") + label).c_str(),
                                 BM_OnDemand, std::string(line));

  struct group_bench {
    const char* label;
    const char* line;
    int max;
  };
  const group_bench groups[] = {
      {"HOSTNAME", "$HOSTNAME:hg_{}:,$", p.hosts},
      {"custom_host", "$_HOSTFOO:hg_{}:,$", p.hosts},
      {"SERVICESTATE", "$SERVICESTATE:sg_{}:,$", p.hosts * p.services_per_host},
      {"CONTACTEMAIL", "$CONTACTEMAIL:cg_{}:,$", k_contacts},
  };
  for (auto& g : groups) {
    auto* b = benchmark::RegisterBenchmark(
        (std::string("BM_Group/") + g.label).c_str(), BM_Group,
        std::string(g.line));
    for (int size : k_group_sizes)
      if (size <= g.max)
        b->Arg(size);
  }

  benchmark::RegisterBenchmark("BM_SummaryComputed", BM_SummaryComputed)
      ->Arg(0)
      ->Arg(1);
  benchmark::RegisterBenchmark("BM_HostTotalsComputed", BM_HostTotalsComputed);
  benchmark::RegisterBenchmark("BM_Cleaning", BM_Cleaning)->Arg(0)->Arg(1);
  benchmark::RegisterBenchmark("BM_ServiceCheckCommand",
                               BM_ServiceCheckCommand);
  benchmark::RegisterBenchmark("BM_HostCheckCommand", BM_HostCheckCommand);
  benchmark::RegisterBenchmark("BM_NotificationLine", BM_NotificationLine);
}

}  // namespace

int main(int argc, char** argv) {
  setenv("TZ", ":Europe/Paris", 1);
  com::centreon::common::log_v2::log_v2::load("engine-macros-bench");
  init_loggers();
  com::centreon::common::pool::load(g_io_context, runtime_logger);
  com::centreon::common::pool::set_pool_size(0);

  p.hosts = env_int("MACROS_BENCH_HOSTS", 1000);
  p.services_per_host = env_int("MACROS_BENCH_SERVICES_PER_HOST", 10);

  auto state_hlp = init_config_state();
  build_poller(*state_hlp);

  /* init_config_state() raises some log levels for the tests; the macros are
   * measured with their production level. */
  for (auto& l : {macros_logger, functions_logger, config_logger, events_logger,
                  checks_logger, notifications_logger})
    l->set_level(spdlog::level::err);

  register_benchmarks();
  benchmark::Initialize(&argc, argv);
  benchmark::AddCustomContext("hosts", std::to_string(p.hosts));
  benchmark::AddCustomContext("services",
                              std::to_string(p.hosts * p.services_per_host));
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();

  deinit_config_state();
  g_io_context->stop();
  com::centreon::common::pool::unload();
  spdlog::shutdown();
  return 0;
}
