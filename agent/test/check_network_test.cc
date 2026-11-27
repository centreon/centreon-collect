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

#include <gtest/gtest.h>

#include "com/centreon/common/rapidjson_helper.hh"
#include "com/centreon/common/utf8.hh"

#include "check_network.hh"

extern std::shared_ptr<asio::io_context> g_io_context;

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_network_detail;
using namespace std::chrono_literals;

using com::centreon::common::perfdata;

namespace {

constexpr uint64_t one_gigabit = 1'000'000'000;

/**
 * @brief fabricated row of the interface table, physical and up by default
 */
MIB_IF_ROW2 create_row(uint64_t luid,
                       const char* name,
                       const char* description,
                       IF_OPER_STATUS status = IfOperStatusUp,
                       uint64_t speed = one_gigabit,
                       bool connector_present = true,
                       bool filter_interface = false) {
  MIB_IF_ROW2 row{};
  row.InterfaceLuid.Value = luid;
  wcsncpy_s(row.Alias, com::centreon::common::utf8_to_wstring(name).c_str(),
            _TRUNCATE);
  wcsncpy_s(row.Description,
            com::centreon::common::utf8_to_wstring(description).c_str(),
            _TRUNCATE);
  row.OperStatus = status;
  row.ReceiveLinkSpeed = speed;
  row.TransmitLinkSpeed = speed;
  row.InterfaceAndOperStatusFlags.ConnectorPresent = connector_present;
  row.InterfaceAndOperStatusFlags.FilterInterface = filter_interface;
  return row;
}

/**
 * @brief set the counters of a row
 */
MIB_IF_ROW2& set_counters(MIB_IF_ROW2& row,
                          uint64_t in_octets,
                          uint64_t out_octets,
                          uint64_t in_errors = 0,
                          uint64_t out_errors = 0,
                          uint64_t in_discards = 0,
                          uint64_t out_discards = 0) {
  row.InOctets = in_octets;
  row.OutOctets = out_octets;
  row.InErrors = in_errors;
  row.OutErrors = out_errors;
  row.InDiscards = in_discards;
  row.OutDiscards = out_discards;
  return row;
}

std::vector<std::string> names(const network_snapshot& snap) {
  std::vector<std::string> ret;
  for (const auto& [luid, itf] : snap.get_interfaces()) {
    ret.push_back(itf.name);
  }
  std::sort(ret.begin(), ret.end());
  return ret;
}

const perfdata* find_perf(const std::list<perfdata>& perfs,
                          std::string_view name) {
  for (const perfdata& perf : perfs) {
    if (perf.name() == name) {
      return &perf;
    }
  }
  return nullptr;
}

/**
 * @brief result received by the completion handler of a check
 */
struct completion_result {
  std::mutex protect;
  std::condition_variable cv;
  bool done = false;
  int status = -1;
  std::list<perfdata> perfs;
  std::list<std::string> outputs;

  check::completion_handler handler() {
    return [this](const std::shared_ptr<check>&, int stat,
                  const std::list<perfdata>& perfdata,
                  const std::list<std::string>& outs) {
      std::lock_guard l(protect);
      status = stat;
      perfs = perfdata;
      outputs = outs;
      done = true;
      cv.notify_all();
    };
  }

  bool wait(std::chrono::seconds timeout) {
    std::unique_lock l(protect);
    return cv.wait_for(l, timeout, [this] { return done; });
  }
};

/**
 * @brief check whose measures come from a function instead of the OS
 */
class check_network_mock : public check_network {
 public:
  std::function<std::unique_ptr<network_snapshot>(const interface_filter&,
                                                  unsigned call_index)>
      measure_impl;
  unsigned nb_measures = 0;

  using check_network::check_network;

  std::unique_ptr<network_snapshot> measure() override {
    return measure_impl(get_filter(), nb_measures++);
  }
};

}  // namespace

class check_network_test : public testing::Test {
 protected:
  Service serv;
  const std::chrono::steady_clock::time_point t0 =
      std::chrono::steady_clock::now();
  const std::shared_ptr<spdlog::logger> logger = spdlog::default_logger();

  check_network_test() {
    serv.set_service_description("serv");
    serv.set_command_name("cmd_name");
    serv.set_command_line("cmd_line");
  }

  std::shared_ptr<check_network> create_check(
      const char* json_args,
      check::completion_handler&& handler = {}) {
    rapidjson::Document args =
        com::centreon::common::rapidjson_helper::read_from_string(json_args);
    if (!handler) {
      handler = [](const std::shared_ptr<check>&, int,
                   const std::list<perfdata>&,
                   const std::list<std::string>&) {};
    }
    return std::make_shared<check_network>(
        g_io_context, spdlog::default_logger(), time_point{}, serv, args,
        nullptr, std::move(handler), std::make_shared<checks_statistics>());
  }
};

TEST_F(check_network_test, default_filter_keeps_physical_interfaces) {
  MIB_IF_ROW2 rows[] = {
      create_row(1, "Ethernet", "Intel(R) Ethernet Connection I219-V"),
      // a filter interface repeats the counters of its adapter, it is
      // skipped even with a connector
      create_row(2, "Ethernet-QoS Packet Scheduler-0000",
                 "Intel(R) Ethernet Connection I219-V-QoS Packet "
                 "Scheduler-0000",
                 IfOperStatusUp, one_gigabit, true, true),
      create_row(3, "vEthernet (Default Switch)",
                 "Hyper-V Virtual Ethernet Adapter", IfOperStatusUp,
                 10 * one_gigabit, false),
      create_row(4, "Loopback Pseudo-Interface 1",
                 "Software Loopback Interface 1", IfOperStatusUp, 1'073'741'824,
                 false)};

  auto checker = create_check("{}");
  network_snapshot snap(t0, rows, checker->get_filter(), logger);

  EXPECT_EQ(names(snap), std::vector<std::string>({"Ethernet"}));
}

TEST_F(check_network_test, physical_only_false_keeps_virtual_interfaces) {
  MIB_IF_ROW2 rows[] = {
      create_row(1, "Ethernet", "Intel(R) Ethernet Connection I219-V"),
      create_row(2, "Ethernet-QoS Packet Scheduler-0000",
                 "Intel(R) Ethernet Connection I219-V-QoS Packet "
                 "Scheduler-0000",
                 IfOperStatusUp, one_gigabit, true, true),
      create_row(3, "vEthernet (Default Switch)",
                 "Hyper-V Virtual Ethernet Adapter", IfOperStatusUp,
                 10 * one_gigabit, false)};

  auto checker = create_check(R"({"physical-only": false})");
  network_snapshot snap(t0, rows, checker->get_filter(), logger);
  EXPECT_EQ(names(snap), std::vector<std::string>(
                             {"Ethernet", "vEthernet (Default Switch)"}));

  // string values are accepted
  checker = create_check(R"({"physical-only": "false"})");
  network_snapshot snap2(t0, rows, checker->get_filter(), logger);
  EXPECT_EQ(snap2.get_interfaces().size(), 2);
}

TEST_F(check_network_test, name_and_description_filters) {
  MIB_IF_ROW2 rows[] = {
      create_row(1, "Ethernet", "Intel(R) Ethernet Connection I219-V"),
      create_row(2, "Ethernet 2", "Intel(R) Ethernet Connection I219-V #2"),
      create_row(3, "Ethernet 3", "Realtek PCIe GbE Family Controller"),
      create_row(4, "Wi-Fi", "Intel(R) Wi-Fi 6 AX201 160MHz"),
      create_row(5, "Ethernet 4", "Intel(R) Virtual Function Adapter")};

  // regexes match the whole string and are case insensitive
  auto checker = create_check(R"({
    "filter-name": "ethernet.*",
    "exclude-name": "ETHERNET 2",
    "filter-description": "intel.*",
    "exclude-description": ".*virtual.*"
  })");
  network_snapshot snap(t0, rows, checker->get_filter(), logger);
  EXPECT_EQ(names(snap), std::vector<std::string>({"Ethernet"}));

  // a partial match is not enough
  checker = create_check(R"({"filter-name": "ether"})");
  network_snapshot snap2(t0, rows, checker->get_filter(), logger);
  EXPECT_TRUE(snap2.get_interfaces().empty());
}

TEST_F(check_network_test, snapshot_counts_excluded_rows) {
  MIB_IF_ROW2 rows[] = {create_row(1, "Ethernet", "Intel(R) Ethernet"),
                        create_row(2, "Wi-Fi", "Intel(R) Wi-Fi")};

  auto checker = create_check(R"({"filter-name": "wi-fi"})");
  network_snapshot snap(t0, rows, checker->get_filter(), logger);
  EXPECT_EQ(snap.get_nb_rows(), 2u);
  EXPECT_EQ(names(snap), std::vector<std::string>({"Wi-Fi"}));
}

TEST_F(check_network_test, non_ascii_names_are_utf8) {
  MIB_IF_ROW2 rows[] = {
      create_row(1, "Connexion réseau", "Contrôleur Ethernet"),
      create_row(2, "Ethernet", "Intel(R) Ethernet Connection I219-V")};

  // RE2 is UTF-8 aware: É matches é
  auto checker = create_check(R"({"filter-name": "CONNEXION RÉSEAU"})");
  network_snapshot first(t0, rows, checker->get_filter(), logger);
  set_counters(rows[0], 1'250'000, 0);
  network_snapshot second(t0 + 1s, rows, checker->get_filter(), logger);

  ASSERT_EQ(names(second), std::vector<std::string>({"Connexion réseau"}));
  EXPECT_EQ(second.get_interfaces().begin()->second.description,
            "Contrôleur Ethernet");

  std::string output;
  std::list<perfdata> perfs;
  checker->compute(first, second, &output, &perfs);
  EXPECT_NE(
      find_perf(perfs, "Connexion réseau#interface.traffic.in.bitspersecond"),
      nullptr);
  EXPECT_NE(output.find("'Connexion réseau' up"), std::string::npos);
}

TEST_F(check_network_test, empty_arguments_are_ignored) {
  // unset macros give empty strings
  auto checker = create_check(R"({
    "filter-name": "", "exclude-name": "", "filter-description": "",
    "exclude-description": "", "physical-only": "", "warning-state": "",
    "critical-state": "", "warning-traffic-in": "", "critical-traffic-in": "",
    "warning-traffic-in-prct": "", "critical-errors-out": ""
  })");

  MIB_IF_ROW2 rows[] = {
      create_row(1, "Ethernet", "Intel(R) Ethernet Connection I219-V",
                 IfOperStatusDown),
      create_row(2, "vEthernet", "Hyper-V Virtual Ethernet Adapter",
                 IfOperStatusUp, one_gigabit, false)};
  network_snapshot first(t0, rows, checker->get_filter(), logger);
  set_counters(rows[0], 125'000'000, 125'000'000, 100, 100);
  network_snapshot second(t0 + 1s, rows, checker->get_filter(), logger);

  std::string output;
  std::list<perfdata> perfs;
  EXPECT_EQ(checker->compute(first, second, &output, &perfs), e_status::ok);
  EXPECT_EQ(output.substr(0, output.find('\n')), "OK: 1 interface: 1 down");
  for (const perfdata& perf : perfs) {
    EXPECT_TRUE(std::isnan(perf.warning())) << perf.name();
    EXPECT_TRUE(std::isnan(perf.critical())) << perf.name();
  }
}

TEST_F(check_network_test, invalid_arguments_throw) {
  EXPECT_THROW(create_check(R"({"filter-name": "("})"), std::exception);
  EXPECT_THROW(create_check(R"({"exclude-description": "[a-"})"),
               std::exception);
  EXPECT_THROW(create_check(R"({"physical-only": "maybe"})"), std::exception);
  EXPECT_THROW(create_check(R"({"warning-traffic-in": "abc"})"),
               std::exception);
  EXPECT_THROW(create_check(R"({"critical-discards-out": "1:2:3"})"),
               std::exception);
  // a state regex that matches no state would never trigger
  EXPECT_THROW(create_check(R"({"critical-state": "dwn"})"), std::exception);
  EXPECT_THROW(create_check(R"({"warning-state": "("})"), std::exception);
  // numbers are accepted as thresholds
  EXPECT_NO_THROW(create_check(R"({"warning-traffic-in": 1000000})"));
  // an unknown argument is only logged
  EXPECT_NO_THROW(create_check(R"({"unknown-arg": "1", "timeout": 10})"));
}

TEST_F(check_network_test, rates_and_perfdata) {
  MIB_IF_ROW2 row = create_row(1, "Ethernet", "Intel(R) Ethernet");
  auto checker = create_check("{}");

  set_counters(row, 1000, 2000, 10, 0, 3, 7);
  network_snapshot first(t0, std::span(&row, 1), checker->get_filter(), logger);
  // in 10 s: 12.5 MB in, 1.25 MB out, 20 errors in, 10 out, 5 discards in
  set_counters(row, 1000 + 12'500'000, 2000 + 1'250'000, 30, 10, 8, 7);
  network_snapshot second(t0 + 10s, std::span(&row, 1), checker->get_filter(),
                          logger);

  std::string output;
  std::list<perfdata> perfs;
  EXPECT_EQ(checker->compute(first, second, &output, &perfs), e_status::ok);
  EXPECT_EQ(output,
            "OK: 1 interface: 1 up\n"
            "'Ethernet' up, speed 1.00 Gb/s, traffic in 10.00 Mb/s (1.00%), "
            "out 1.00 Mb/s (0.10%), errors in 2.00/s, out 1.00/s, discards "
            "in 0.50/s, out 0.00/s");

  struct expected_perf {
    std::string_view name;
    std::string_view unit;
    double value;
    double max;
  };
  const expected_perf expected[] = {
      {"Ethernet#interface.traffic.in.bitspersecond", "b/s", 10'000'000,
       one_gigabit},
      {"Ethernet#interface.traffic.out.bitspersecond", "b/s", 1'000'000,
       one_gigabit},
      {"Ethernet#interface.traffic.in.percentage", "%", 1.0, 100},
      {"Ethernet#interface.traffic.out.percentage", "%", 0.1, 100},
      {"Ethernet#interface.packets.in.error.persecond", "packets/s", 2.0, NAN},
      {"Ethernet#interface.packets.out.error.persecond", "packets/s", 1.0, NAN},
      {"Ethernet#interface.packets.in.discard.persecond", "packets/s", 0.5,
       NAN},
      {"Ethernet#interface.packets.out.discard.persecond", "packets/s", 0.0,
       NAN}};

  ASSERT_EQ(perfs.size(), std::size(expected));
  for (const expected_perf& exp : expected) {
    const perfdata* perf = find_perf(perfs, exp.name);
    ASSERT_NE(perf, nullptr) << exp.name;
    EXPECT_EQ(perf->unit(), exp.unit) << exp.name;
    EXPECT_NEAR(perf->value(), exp.value, exp.value * 1e-6) << exp.name;
    EXPECT_EQ(perf->min(), 0) << exp.name;
    if (std::isnan(exp.max)) {
      EXPECT_TRUE(std::isnan(perf->max())) << exp.name;
    } else {
      EXPECT_EQ(perf->max(), static_cast<float>(exp.max)) << exp.name;
    }
  }
}

TEST_F(check_network_test, unknown_speed_has_no_percentage) {
  MIB_IF_ROW2 rows[] = {
      create_row(1, "Ethernet", "Intel(R) Ethernet", IfOperStatusUp, 0),
      create_row(2, "Ethernet 2", "Intel(R) Ethernet #2", IfOperStatusUp,
                 NET_IF_LINK_SPEED_UNKNOWN)};
  auto checker = create_check(R"({"warning-traffic-in-prct": "0"})");

  network_snapshot first(t0, rows, checker->get_filter(), logger);
  set_counters(rows[0], 1'250'000, 0);
  set_counters(rows[1], 1'250'000, 0);
  network_snapshot second(t0 + 1s, rows, checker->get_filter(), logger);

  std::string output;
  std::list<perfdata> perfs;
  // the percentage threshold can't trigger without a speed
  EXPECT_EQ(checker->compute(first, second, &output, &perfs), e_status::ok);
  EXPECT_EQ(output,
            "OK: 2 interfaces: 2 up\n"
            "'Ethernet' up, speed unknown, traffic in 10.00 Mb/s, out 0.00 "
            "b/s, errors in 0.00/s, out 0.00/s, discards in 0.00/s, out "
            "0.00/s\n"
            "'Ethernet 2' up, speed unknown, traffic in 10.00 Mb/s, out 0.00 "
            "b/s, errors in 0.00/s, out 0.00/s, discards in 0.00/s, out "
            "0.00/s");
  EXPECT_EQ(perfs.size(), 12);
  for (const perfdata& perf : perfs) {
    EXPECT_EQ(perf.name().find("percentage"), std::string::npos);
  }
  EXPECT_TRUE(std::isnan(
      find_perf(perfs, "Ethernet#interface.traffic.in.bitspersecond")->max()));
}

TEST_F(check_network_test, metric_thresholds) {
  MIB_IF_ROW2 row = create_row(1, "Ethernet", "Intel(R) Ethernet");
  auto checker = create_check(R"({
    "warning-traffic-in-prct": "0.5",
    "critical-traffic-in-prct": 5,
    "critical-errors-in": "1",
    "warning-discards-in": "0.4",
    "critical-traffic-out": "@0:100"
  })");

  network_snapshot first(t0, std::span(&row, 1), checker->get_filter(), logger);
  set_counters(row, 12'500'000, 1'250'000, 20, 0, 5, 0);
  network_snapshot second(t0 + 10s, std::span(&row, 1), checker->get_filter(),
                          logger);

  std::string output;
  std::list<perfdata> perfs;
  EXPECT_EQ(checker->compute(first, second, &output, &perfs),
            e_status::critical);
  EXPECT_EQ(output.substr(0, output.find('\n')),
            "CRITICAL: 'Ethernet' errors in is 2.00/s WARNING: 'Ethernet' "
            "traffic in is 1.00%, discards in is 0.50/s");

  const perfdata* prct =
      find_perf(perfs, "Ethernet#interface.traffic.in.percentage");
  ASSERT_NE(prct, nullptr);
  EXPECT_EQ(prct->warning(), 0.5f);
  EXPECT_EQ(prct->critical(), 5.0f);

  const perfdata* errors =
      find_perf(perfs, "Ethernet#interface.packets.in.error.persecond");
  ASSERT_NE(errors, nullptr);
  EXPECT_TRUE(std::isnan(errors->warning()));
  EXPECT_EQ(errors->critical(), 1.0f);

  // inclusive range not triggered by 1 Mb/s, but reported
  const perfdata* traffic_out =
      find_perf(perfs, "Ethernet#interface.traffic.out.bitspersecond");
  ASSERT_NE(traffic_out, nullptr);
  EXPECT_EQ(traffic_out->critical_low(), 0.0f);
  EXPECT_EQ(traffic_out->critical(), 100.0f);
  EXPECT_TRUE(traffic_out->critical_mode());
}

TEST_F(check_network_test, warning_only_output) {
  MIB_IF_ROW2 rows[] = {create_row(1, "Ethernet", "Intel(R) Ethernet"),
                        create_row(2, "Ethernet 2", "Intel(R) Ethernet #2")};
  auto checker = create_check(R"({"warning-traffic-out": "1000"})");

  network_snapshot first(t0, rows, checker->get_filter(), logger);
  set_counters(rows[0], 0, 1250);
  set_counters(rows[1], 0, 2500);
  network_snapshot second(t0 + 1s, rows, checker->get_filter(), logger);

  std::string output;
  std::list<perfdata> perfs;
  EXPECT_EQ(checker->compute(first, second, &output, &perfs),
            e_status::warning);
  EXPECT_EQ(output.substr(0, output.find('\n')),
            "WARNING: 'Ethernet' traffic out is 10.00 Kb/s, 'Ethernet 2' "
            "traffic out is 20.00 Kb/s");
}

TEST_F(check_network_test, state_thresholds) {
  MIB_IF_ROW2 rows[] = {
      create_row(1, "Ethernet", "Intel(R) Ethernet"),
      create_row(2, "Ethernet 2", "Intel(R) Ethernet #2", IfOperStatusDown, 0),
      create_row(3, "Wi-Fi", "Intel(R) Wi-Fi", IfOperStatusNotPresent, 0)};

  auto checker = create_check(
      R"({"critical-state": "down", "warning-state": "NotPresent|dormant"})");
  network_snapshot first(t0, rows, checker->get_filter(), logger);
  network_snapshot second(t0 + 1s, rows, checker->get_filter(), logger);

  std::string output;
  std::list<perfdata> perfs;
  EXPECT_EQ(checker->compute(first, second, &output, &perfs),
            e_status::critical);
  EXPECT_EQ(output.substr(0, output.find('\n')),
            "CRITICAL: 'Ethernet 2' is down WARNING: 'Wi-Fi' is notpresent");

  // without state threshold, states are only counted
  checker = create_check("{}");
  output.clear();
  perfs.clear();
  EXPECT_EQ(checker->compute(first, second, &output, &perfs), e_status::ok);
  EXPECT_EQ(output.substr(0, output.find('\n')),
            "OK: 3 interfaces: 1 up, 1 down, 1 notpresent");
}

TEST_F(check_network_test, no_interface_is_unknown) {
  auto checker = create_check("{}");
  network_snapshot first(t0, {}, checker->get_filter(), logger);
  network_snapshot second(t0 + 1s, {}, checker->get_filter(), logger);

  std::string output;
  std::list<perfdata> perfs;
  EXPECT_EQ(checker->compute(first, second, &output, &perfs),
            e_status::unknown);
  EXPECT_EQ(output, "UNKNOWN: no network interface matches the filters");
  EXPECT_TRUE(perfs.empty());
}

TEST_F(check_network_test, no_elapsed_time_is_unknown) {
  MIB_IF_ROW2 row = create_row(1, "Ethernet", "Intel(R) Ethernet");
  auto checker = create_check("{}");
  network_snapshot first(t0, std::span(&row, 1), checker->get_filter(), logger);

  std::string output;
  std::list<perfdata> perfs;
  EXPECT_EQ(checker->compute(first, first, &output, &perfs), e_status::unknown);
  EXPECT_EQ(output, "UNKNOWN: no time elapsed between the two measures");
}

TEST_F(check_network_test, interface_appearing_during_measure_is_ignored) {
  MIB_IF_ROW2 rows[] = {create_row(1, "Ethernet", "Intel(R) Ethernet"),
                        create_row(2, "Ethernet 2", "Intel(R) Ethernet #2")};
  auto checker = create_check("{}");
  network_snapshot first(t0, std::span(rows, 1), checker->get_filter(), logger);
  network_snapshot second(t0 + 1s, rows, checker->get_filter(), logger);

  std::string output;
  std::list<perfdata> perfs;
  EXPECT_EQ(checker->compute(first, second, &output, &perfs), e_status::ok);
  EXPECT_EQ(output.substr(0, output.find('\n')), "OK: 1 interface: 1 up");
  for (const perfdata& perf : perfs) {
    EXPECT_EQ(perf.name().rfind("Ethernet#", 0), 0) << perf.name();
  }
}

TEST_F(check_network_test, counter_reset_uses_counts_since_reset) {
  MIB_IF_ROW2 row = create_row(1, "Ethernet", "Intel(R) Ethernet");
  auto checker = create_check("{}");
  set_counters(row, 1'000'000'000, 0);
  network_snapshot first(t0, std::span(&row, 1), checker->get_filter(), logger);
  set_counters(row, 1'250'000, 0);
  network_snapshot second(t0 + 10s, std::span(&row, 1), checker->get_filter(),
                          logger);

  std::string output;
  std::list<perfdata> perfs;
  checker->compute(first, second, &output, &perfs);
  const perfdata* traffic_in =
      find_perf(perfs, "Ethernet#interface.traffic.in.bitspersecond");
  ASSERT_NE(traffic_in, nullptr);
  EXPECT_FLOAT_EQ(traffic_in->value(), 1'000'000);
}

TEST_F(check_network_test, start_check_completes_after_second_measure) {
  completion_result result;
  rapidjson::Document args =
      com::centreon::common::rapidjson_helper::read_from_string(
          R"({"warning-traffic-in": "1000"})");
  auto checker = std::make_shared<check_network_mock>(
      g_io_context, spdlog::default_logger(), time_point{}, serv, args, nullptr,
      result.handler(), std::make_shared<checks_statistics>());

  checker->measure_impl = [this](const interface_filter& filter,
                                 unsigned call_index) {
    MIB_IF_ROW2 row = create_row(1, "Ethernet", "Intel(R) Ethernet");
    set_counters(row, call_index * 1250, 0);
    return std::make_unique<network_snapshot>(
        t0 + call_index * 1s, std::span(&row, 1), filter, logger);
  };

  asio::post(*g_io_context, [checker] { checker->start_check(2s); });

  ASSERT_TRUE(result.wait(10s));
  EXPECT_EQ(checker->nb_measures, 2);
  EXPECT_EQ(result.status, e_status::warning);
  ASSERT_EQ(result.outputs.size(), 1);
  EXPECT_EQ(result.outputs.front().substr(0, result.outputs.front().find('\n')),
            "WARNING: 'Ethernet' traffic in is 10.00 Kb/s");
  EXPECT_EQ(result.perfs.size(), 8);
}

TEST_F(check_network_test, measure_failure_is_unknown) {
  completion_result result;
  rapidjson::Document args =
      com::centreon::common::rapidjson_helper::read_from_string("{}");
  auto checker = std::make_shared<check_network_mock>(
      g_io_context, spdlog::default_logger(), time_point{}, serv, args, nullptr,
      result.handler(), std::make_shared<checks_statistics>());

  checker->measure_impl = [](const interface_filter&,
                             unsigned) -> std::unique_ptr<network_snapshot> {
    throw com::centreon::exceptions::msg_fmt(
        "failed to read the network interface table (GetIfTable2Ex): "
        "access denied");
  };

  asio::post(*g_io_context, [checker] { checker->start_check(2s); });

  ASSERT_TRUE(result.wait(10s));
  EXPECT_EQ(result.status, e_status::unknown);
  ASSERT_EQ(result.outputs.size(), 1);
  EXPECT_EQ(result.outputs.front(),
            "UNKNOWN: failed to read the network interface table "
            "(GetIfTable2Ex): access denied");
  EXPECT_TRUE(result.perfs.empty());
}

TEST_F(check_network_test, get_if_table2_ex_duration) {
  PMIB_IF_TABLE2 table = nullptr;
  const auto start = std::chrono::steady_clock::now();
  const DWORD result = GetIfTable2Ex(MibIfTableNormal, &table);
  const std::chrono::duration<double, std::milli> elapsed =
      std::chrono::steady_clock::now() - start;
  const std::unique_ptr<MIB_IF_TABLE2, decltype(&FreeMibTable)> table_owner(
      table, &FreeMibTable);

  fmt::print("GetIfTable2Ex(MibIfTableNormal) took {:.3f} ms\n",
             elapsed.count());
  ASSERT_EQ(result, NO_ERROR);
  EXPECT_NE(table_owner, nullptr);
}

/**
 * the interface table of the host is read, values are only checked to be
 * well formed
 */
TEST_F(check_network_test, host_interfaces_are_well_formed) {
  completion_result result;
  // the loopback interface exists on every host
  auto checker = create_check(R"({"physical-only": false})", result.handler());

  asio::post(*g_io_context, [checker] { checker->start_check(2s); });

  ASSERT_TRUE(result.wait(10s));
  EXPECT_EQ(result.status, e_status::ok);
  ASSERT_EQ(result.outputs.size(), 1);
  fmt::print("{}\n", result.outputs.front());
  EXPECT_EQ(result.outputs.front().rfind("OK: ", 0), 0)
      << result.outputs.front();
  EXPECT_FALSE(result.perfs.empty());
  for (const perfdata& perf : result.perfs) {
    EXPECT_NE(perf.name().find("#interface."), std::string::npos)
        << perf.name();
    EXPECT_GE(perf.value(), 0) << perf.name();
  }
}
