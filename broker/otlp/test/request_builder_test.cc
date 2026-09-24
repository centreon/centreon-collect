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

#include "com/centreon/broker/otlp/request_builder.hh"

#include <gtest/gtest.h>
#include <spdlog/common.h>
#include <chrono>
#include <thread>

#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;
using log_v2 = com::centreon::common::log_v2::log_v2;

namespace {

/* The broker ships no mapping table: the tests bring the rules they check. */
constexpr std::string_view k_test_table = R"({
  "metrics": {
    "rta": { "name": "centreon.icmp.rtt", "unit": "s", "scale": 0.001 },
    "pl": { "name": "centreon.icmp.packet_loss", "unit": "1", "scale": 0.01 },
    "disk.space.usage.bytes": {
      "name": "system.filesystem.usage", "unit": "By",
      "instrument": "sum_non_monotonic",
      "attributes": { "system.filesystem.state": "used" },
      "instance_attribute": "system.filesystem.mountpoint"
    }
  }
})";

/* Names without a memory-mapped cache: the applier state is not loaded under
 * unit tests, so global_cache would have nothing to resolve. */
class fake_enricher : public resource_enricher {
 public:
  absl::flat_hash_map<uint64_t, std::string> hosts;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, std::string> services;

  std::optional<std::string> host_name(uint64_t host_id) override {
    auto it = hosts.find(host_id);
    if (it == hosts.end())
      return std::nullopt;
    return it->second;
  }
  std::optional<std::string> service_description(uint64_t h,
                                                 uint64_t s) override {
    auto it = services.find({h, s});
    if (it == services.end())
      return std::nullopt;
    return it->second;
  }
};

class RequestBuilderTest : public ::testing::Test {
 public:
  std::shared_ptr<fake_enricher> enricher;
  otlp_config::pointer conf;
  std::shared_ptr<spdlog::logger> logger;
  mapping_provider::pointer mapping;

  void SetUp() override {
    enricher = std::make_shared<fake_enricher>();
    enricher->hosts[42] = "srv-web-01";
    enricher->services[{42, 7}] = "Disk-/var";
    conf = std::make_shared<otlp_config>();
    logger = log_v2::instance().get(log_v2::OTL);
    logger->set_level(spdlog::level::trace);
    mapping = std::make_shared<mapping_provider>(
        std::filesystem::path(), mapping_table::from_json(k_test_table),
        logger);
  }

  ServiceStatus make_status(const std::string& perfdata) {
    ServiceStatus ss;
    ss.set_host_id(42);
    ss.set_service_id(7);
    ss.set_last_check(1700000000);
    ss.set_state(ServiceStatus::WARNING);
    ss.set_state_type(ServiceStatus::HARD);
    ss.set_perfdata(perfdata);
    return ss;
  }

  HostStatus make_host_status(const std::string& perfdata) {
    HostStatus hs;
    hs.set_host_id(42);
    hs.set_last_check(1700000000);
    hs.set_state(HostStatus::DOWN);
    hs.set_state_type(HostStatus::HARD);
    hs.set_perfdata(perfdata);
    return hs;
  }

  /* Find a metric by name anywhere in the request. */
  static const ::opentelemetry::proto::metrics::v1::Metric* find_metric(
      const ::opentelemetry::proto::collector::metrics::v1::
          ExportMetricsServiceRequest& req,
      const std::string& name) {
    for (const auto& rm : req.resource_metrics())
      for (const auto& sm : rm.scope_metrics())
        for (const auto& m : sm.metrics())
          if (m.name() == name)
            return &m;
    return nullptr;
  }

  static std::string attr(
      const ::opentelemetry::proto::metrics::v1::NumberDataPoint& dp,
      const std::string& key) {
    for (const auto& a : dp.attributes())
      if (a.key() == key)
        return a.value().string_value();
    return {};
  }

  static std::string resource_attr(
      const ::opentelemetry::proto::metrics::v1::ResourceMetrics& rm,
      const std::string& key) {
    for (const auto& a : rm.resource().attributes())
      if (a.key() == key)
        return a.value().string_value();
    return {};
  }
  static bool has_resource_attr(
      const ::opentelemetry::proto::metrics::v1::ResourceMetrics& rm,
      const std::string& key) {
    for (const auto& a : rm.resource().attributes())
      if (a.key() == key)
        return true;
    return false;
  }

  static std::vector<std::string> resource_array_attr(
      const ::opentelemetry::proto::metrics::v1::ResourceMetrics& rm,
      const std::string& key) {
    std::vector<std::string> ret;
    for (const auto& a : rm.resource().attributes())
      if (a.key() == key)
        for (const auto& v : a.value().array_value().values())
          ret.push_back(v.string_value());
    return ret;
  }

  static AgentHostInfo make_host_info(const std::string& machine_id) {
    AgentHostInfo info;
    info.set_poller_id(1);
    info.set_host_id(42);
    info.set_host_name("srv-web-01");
    info.set_observed_at(std::time(nullptr));
    info.set_os_type("linux");
    info.set_os_name("AlmaLinux");
    info.set_os_version("9.4");
    info.set_arch("amd64");
    info.set_machine_id(machine_id);
    info.add_ips("10.0.0.1");
    info.add_ips("169.254.3.4");
    info.add_ips("fe80::1");
    return info;
  }
};

}  // namespace

/* The acceptance criterion: host.name on the resource, so a backend can join
 * these metrics with CLM logs from the same host. */
TEST_F(RequestBuilderTest, emits_host_name_as_resource_attribute) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_service_status(make_status("used_prct=87%;90;95;0;100")));

  const auto& req = b.peek();
  ASSERT_EQ(req.resource_metrics_size(), 1);
  EXPECT_EQ(resource_attr(req.resource_metrics(0), "host.name"), "srv-web-01");
}

/* service.name must describe the emitter. Putting the Centreon check name
 * there would make every check its own Prometheus job. */
TEST_F(RequestBuilderTest, service_name_identifies_the_emitter_not_the_check) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms")));

  const auto& rm = b.peek().resource_metrics(0);
  EXPECT_EQ(resource_attr(rm, "service.name"), "centreon-broker");
  EXPECT_EQ(resource_attr(rm, "service.namespace"), "centreon");
  EXPECT_NE(resource_attr(rm, "service.name"), "Disk-/var");
}

TEST_F(RequestBuilderTest, centreon_service_is_a_datapoint_attribute) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms")));

  const auto* m = find_metric(b.peek(), "centreon.icmp.rtt");
  ASSERT_NE(m, nullptr);
  ASSERT_EQ(m->gauge().data_points_size(), 1);
  const auto& dp = m->gauge().data_points(0);
  EXPECT_EQ(attr(dp, "centreon.service.description"), "Disk-/var");
  /* ms -> s */
  EXPECT_DOUBLE_EQ(dp.as_double(), 0.25);
  EXPECT_EQ(dp.time_unix_nano(), 1700000000ULL * 1000000000ULL);
}

/* All of a host's series must share one ResourceMetrics, otherwise host-level
 * correlation fragments. */
TEST_F(RequestBuilderTest, one_resource_per_host) {
  enricher->hosts[43] = "srv-web-02";
  request_builder b(conf, enricher, mapping, logger);

  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms")));
  ServiceStatus other = make_status("pl=10%");
  other.set_service_id(8);
  ASSERT_TRUE(b.add_service_status(other));

  ServiceStatus far = make_status("rta=100ms");
  far.set_host_id(43);
  ASSERT_TRUE(b.add_service_status(far));

  EXPECT_EQ(b.peek().resource_metrics_size(), 2);
}

TEST_F(RequestBuilderTest, raw_metric_name_is_always_preserved) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(
      b.add_service_status(make_status("'/var#disk.space.usage.bytes'=1024B")));

  const auto* m = find_metric(b.peek(), "system.filesystem.usage");
  ASSERT_NE(m, nullptr);
  ASSERT_GT(m->sum().data_points_size(), 0);
  EXPECT_EQ(attr(m->sum().data_points(0), "centreon.metric.name"),
            "/var#disk.space.usage.bytes");
  EXPECT_EQ(attr(m->sum().data_points(0), "system.filesystem.mountpoint"),
            "/var");
}

/* Without a mapping file, every metric is exported under centreon.*. */
TEST_F(RequestBuilderTest, no_mapping_exports_everything_under_centreon) {
  request_builder b(conf, enricher, mapping_provider::empty(logger), logger);
  ASSERT_TRUE(b.add_service_status(
      make_status("rta=250ms '/var#disk.space.usage.bytes'=1024B")));

  const auto* rta = find_metric(b.peek(), "centreon.rta");
  ASSERT_NE(rta, nullptr);
  EXPECT_EQ(rta->unit(), "ms");
  ASSERT_EQ(rta->gauge().data_points_size(), 1);
  /* no rule, no rescaling */
  EXPECT_DOUBLE_EQ(rta->gauge().data_points(0).as_double(), 250.0);

  const auto* disk =
      find_metric(b.peek(), "centreon.disk.space.usage.bytes");
  ASSERT_NE(disk, nullptr);
  ASSERT_EQ(disk->gauge().data_points_size(), 1);
  EXPECT_EQ(attr(disk->gauge().data_points(0), "centreon.metric.instance"),
            "/var");
  EXPECT_EQ(find_metric(b.peek(), "system.filesystem.usage"), nullptr);
}

/* ------------------------------------------------------------------ */
/* Optional annotation streams                                         */
/* ------------------------------------------------------------------ */

TEST_F(RequestBuilderTest, thresholds_are_emitted_by_default) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms;500;1000")));

  const auto* t = find_metric(b.peek(), "centreon.icmp.rtt.threshold");
  ASSERT_NE(t, nullptr) << "thresholds default to on";
  /* Same unit as the value it annotates, so the series stays unit-correct. */
  EXPECT_EQ(t->unit(), "s");

  /* Nagios "warn=500" denotes the range 0..500, so the parser yields both a
   * low and a high bound and both are emitted, distinguished by
   * centreon.threshold.bound. */
  bool warn_upper = false, crit_upper = false;
  bool warn_lower = false, crit_lower = false;
  for (const auto& dp : t->gauge().data_points()) {
    const std::string level = attr(dp, "centreon.threshold.level");
    const std::string bound = attr(dp, "centreon.threshold.bound");
    if (level == "warning" && bound == "upper") {
      warn_upper = true;
      EXPECT_DOUBLE_EQ(dp.as_double(), 0.5) << "500ms must scale to 0.5s";
    }
    if (level == "critical" && bound == "upper") {
      crit_upper = true;
      EXPECT_DOUBLE_EQ(dp.as_double(), 1.0) << "1000ms must scale to 1s";
    }
    if (level == "warning" && bound == "lower")
      warn_lower = true;
    if (level == "critical" && bound == "lower")
      crit_lower = true;
  }
  EXPECT_TRUE(warn_upper);
  EXPECT_TRUE(crit_upper);
  EXPECT_TRUE(warn_lower);
  EXPECT_TRUE(crit_lower);
}

TEST_F(RequestBuilderTest, thresholds_can_be_disabled) {
  conf->send_thresholds = false;
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms;500;1000")));
  EXPECT_EQ(find_metric(b.peek(), "centreon.icmp.rtt.threshold"), nullptr);
}

TEST_F(RequestBuilderTest, absent_thresholds_emit_nothing) {
  request_builder b(conf, enricher, mapping, logger);
  /* No warn/crit in the perfdata: nothing to annotate. */
  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms")));
  const auto* t = find_metric(b.peek(), "centreon.icmp.rtt.threshold");
  if (t)
    EXPECT_EQ(t->gauge().data_points_size(), 0);
}

TEST_F(RequestBuilderTest, state_is_emitted_by_default) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms")));

  const auto* s = find_metric(b.peek(), "centreon.check.state");
  ASSERT_NE(s, nullptr);
  ASSERT_EQ(s->gauge().data_points_size(), 1);
  EXPECT_DOUBLE_EQ(s->gauge().data_points(0).as_double(),
                   static_cast<double>(ServiceStatus::WARNING));
  EXPECT_EQ(attr(s->gauge().data_points(0), "centreon.state.type"), "hard");
}

TEST_F(RequestBuilderTest, state_can_be_disabled) {
  conf->send_status = false;
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms")));
  EXPECT_EQ(find_metric(b.peek(), "centreon.check.state"), nullptr);
}

TEST_F(RequestBuilderTest, min_max_bounds_are_emitted_by_default) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_service_status(make_status("pl=10%;40;80;0;100")));

  const auto* bd = find_metric(b.peek(), "centreon.icmp.packet_loss.bound");
  ASSERT_NE(bd, nullptr);
  bool mn = false, mx = false;
  for (const auto& dp : bd->gauge().data_points()) {
    if (attr(dp, "centreon.bound.type") == "min") {
      mn = true;
      EXPECT_DOUBLE_EQ(dp.as_double(), 0.0);
    }
    if (attr(dp, "centreon.bound.type") == "max") {
      mx = true;
      /* Percent scaling applies to bounds too. */
      EXPECT_DOUBLE_EQ(dp.as_double(), 1.0);
    }
  }
  EXPECT_TRUE(mn);
  EXPECT_TRUE(mx);
}

/* ------------------------------------------------------------------ */
/* Host checks                                                         */
/* ------------------------------------------------------------------ */

/* A host check has perfdata too (typically the ping of check_icmp). */
TEST_F(RequestBuilderTest, host_perfdata_is_exported) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_host_status(make_host_status("rta=250ms;500;1000;0 pl=0%")));

  ASSERT_EQ(b.peek().resource_metrics_size(), 1);
  EXPECT_EQ(resource_attr(b.peek().resource_metrics(0), "host.name"),
            "srv-web-01");

  const auto* m = find_metric(b.peek(), "centreon.icmp.rtt");
  ASSERT_NE(m, nullptr);
  ASSERT_EQ(m->gauge().data_points_size(), 1);
  const auto& dp = m->gauge().data_points(0);
  EXPECT_DOUBLE_EQ(dp.as_double(), 0.25);
  EXPECT_EQ(dp.time_unix_nano(), 1700000000ULL * 1000000000ULL);
  EXPECT_EQ(attr(dp, "centreon.metric.name"), "rta");
  /* no service: the resource alone identifies the host */
  for (const auto& a : dp.attributes())
    EXPECT_NE(a.key().rfind("centreon.service.", 0), 0u) << a.key();

  EXPECT_NE(find_metric(b.peek(), "centreon.icmp.packet_loss"), nullptr);
  EXPECT_NE(find_metric(b.peek(), "centreon.icmp.rtt.threshold"), nullptr);
  EXPECT_NE(find_metric(b.peek(), "centreon.icmp.rtt.bound"), nullptr);

  const auto* s = find_metric(b.peek(), "centreon.host.state");
  ASSERT_NE(s, nullptr);
  EXPECT_DOUBLE_EQ(s->gauge().data_points(0).as_double(),
                   static_cast<double>(HostStatus::DOWN));
}

/* send_status only gates the state; the host perfdata still goes out. */
TEST_F(RequestBuilderTest, host_perfdata_is_exported_without_state) {
  conf->send_status = false;
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_host_status(make_host_status("rta=250ms")));
  EXPECT_NE(find_metric(b.peek(), "centreon.icmp.rtt"), nullptr);
  EXPECT_EQ(find_metric(b.peek(), "centreon.host.state"), nullptr);
}

/* Host and service series of a host share its ResourceMetrics. */
TEST_F(RequestBuilderTest, host_and_service_perfdata_share_the_resource) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_host_status(make_host_status("rta=100ms")));
  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms")));

  ASSERT_EQ(b.peek().resource_metrics_size(), 1);
  const auto* m = find_metric(b.peek(), "centreon.icmp.rtt");
  ASSERT_NE(m, nullptr);
  ASSERT_EQ(m->gauge().data_points_size(), 2);
  auto has_service_id = [](const auto& dp) {
    for (const auto& a : dp.attributes())
      if (a.key() == "centreon.service.id")
        return true;
    return false;
  };
  EXPECT_FALSE(has_service_id(m->gauge().data_points(0)));
  EXPECT_TRUE(has_service_id(m->gauge().data_points(1)));
}

TEST_F(RequestBuilderTest, unknown_host_status_is_dropped) {
  request_builder b(conf, enricher, mapping, logger);
  HostStatus orphan = make_host_status("rta=250ms");
  orphan.set_host_id(999);
  EXPECT_FALSE(b.add_host_status(orphan));
  EXPECT_EQ(b.dropped_no_host_name(), 1u);
  EXPECT_EQ(b.peek().resource_metrics_size(), 0);
}

/* ------------------------------------------------------------------ */
/* Unresolvable host                                                   */
/* ------------------------------------------------------------------ */

TEST_F(RequestBuilderTest, unknown_host_is_dropped_not_emitted) {
  request_builder b(conf, enricher, mapping, logger);
  ServiceStatus orphan = make_status("rta=250ms");
  orphan.set_host_id(999);

  EXPECT_FALSE(b.add_service_status(orphan));
  EXPECT_EQ(b.dropped_no_host_name(), 1u);
  /* Emitting without host.name would break correlation and pollute the
   * backend with unattributable series. */
  EXPECT_EQ(b.peek().resource_metrics_size(), 0);
}

TEST_F(RequestBuilderTest, take_resets_the_builder) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms")));
  EXPECT_GT(b.nb_data(), 0u);

  auto req = b.take();
  EXPECT_GT(req.resource_metrics_size(), 0);
  EXPECT_EQ(b.nb_data(), 0u);
  EXPECT_TRUE(b.empty());
  EXPECT_EQ(b.peek().resource_metrics_size(), 0);
}

/* gauge and sum share a protobuf oneof. If the same metric name is reached
 * once as a gauge and once as a sum, switching the oneof would clear the
 * field and throw away the datapoints already collected. */
TEST_F(RequestBuilderTest, conflicting_instrument_does_not_discard_datapoints) {
  request_builder b(conf, enricher, mapping, logger);

  /* Unknown name, so the instrument comes from the perfdata value type: 'c'
   * makes it a counter, plain makes it a gauge. */
  ASSERT_TRUE(b.add_service_status(make_status("custom_thing=5")));
  ServiceStatus as_counter = make_status("custom_thing=9c");
  as_counter.set_service_id(8);
  ASSERT_TRUE(b.add_service_status(as_counter));

  const auto* m = find_metric(b.peek(), "centreon.custom_thing");
  ASSERT_NE(m, nullptr);
  const int total = m->gauge().data_points_size() + m->sum().data_points_size();
  EXPECT_EQ(total, 2) << "no datapoint may be lost to a oneof switch";
}

/* A second status on the same host must extend the existing Metric rather
 * than create a duplicate entry with the same name. */
TEST_F(RequestBuilderTest, repeated_metric_reuses_one_metric_entry) {
  request_builder b(conf, enricher, mapping, logger);
  ASSERT_TRUE(b.add_service_status(make_status("rta=250ms")));
  ServiceStatus again = make_status("rta=300ms");
  again.set_service_id(9);
  ASSERT_TRUE(b.add_service_status(again));

  int count = 0;
  for (const auto& rm : b.peek().resource_metrics())
    for (const auto& sm : rm.scope_metrics())
      for (const auto& m : sm.metrics())
        if (m.name() == "centreon.icmp.rtt")
          ++count;
  EXPECT_EQ(count, 1);

  const auto* m = find_metric(b.peek(), "centreon.icmp.rtt");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->gauge().data_points_size(), 2);
}

/* Hosts monitored by a Centreon Monitoring Agent get the OTel host and os
 * resource attributes it collected. */
TEST_F(RequestBuilderTest, cma_host_metadata_is_on_the_resource) {
  auto store = std::make_shared<host_metadata_store>(std::chrono::seconds(900));
  store->update(make_host_info("4c4c4544004d3510804bb4c04f4a3432"),
                std::time(nullptr));
  request_builder b(conf, enricher, mapping, logger, store);
  ASSERT_TRUE(b.add_service_status(make_status("rta=1ms")));

  const auto& rm = b.peek().resource_metrics(0);
  EXPECT_EQ(resource_attr(rm, "host.name"), "srv-web-01");
  EXPECT_EQ(resource_attr(rm, "host.id"), "4c4c4544004d3510804bb4c04f4a3432");
  EXPECT_EQ(resource_attr(rm, "host.arch"), "amd64");
  EXPECT_EQ(resource_attr(rm, "os.type"), "linux");
  EXPECT_EQ(resource_attr(rm, "os.name"), "AlmaLinux");
  EXPECT_EQ(resource_attr(rm, "os.version"), "9.4");
  EXPECT_EQ(resource_array_attr(rm, "host.ip"),
            (std::vector<std::string>{"10.0.0.1", "169.254.3.4", "fe80::1"}));
  EXPECT_FALSE(has_resource_attr(rm, "host.type"));
}

TEST_F(RequestBuilderTest, link_local_ips_can_be_excluded) {
  conf->host_ip_exclude_link_local = true;
  auto store = std::make_shared<host_metadata_store>(std::chrono::seconds(900));
  store->update(make_host_info("m1"), std::time(nullptr));
  request_builder b(conf, enricher, mapping, logger, store);
  ASSERT_TRUE(b.add_service_status(make_status("rta=1ms")));
  EXPECT_EQ(resource_array_attr(b.peek().resource_metrics(0), "host.ip"),
            std::vector<std::string>{"10.0.0.1"});
}

/* An older agent sends no machine id nor arch: nothing is emitted rather than
 * an empty value. */
TEST_F(RequestBuilderTest, empty_host_metadata_fields_are_not_emitted) {
  auto store = std::make_shared<host_metadata_store>(std::chrono::seconds(900));
  AgentHostInfo info = make_host_info("");
  info.clear_arch();
  info.clear_ips();
  store->update(info, std::time(nullptr));
  request_builder b(conf, enricher, mapping, logger, store);
  ASSERT_TRUE(b.add_service_status(make_status("rta=1ms")));
  const auto& rm = b.peek().resource_metrics(0);
  EXPECT_FALSE(has_resource_attr(rm, "host.id"));
  EXPECT_FALSE(has_resource_attr(rm, "host.arch"));
  EXPECT_FALSE(has_resource_attr(rm, "host.ip"));
  EXPECT_EQ(resource_attr(rm, "os.type"), "linux");
}

TEST_F(RequestBuilderTest, host_without_metadata_keeps_the_basic_resource) {
  auto store = std::make_shared<host_metadata_store>(std::chrono::seconds(900));
  request_builder b(conf, enricher, mapping, logger, store);
  ASSERT_TRUE(b.add_service_status(make_status("rta=1ms")));
  const auto& rm = b.peek().resource_metrics(0);
  EXPECT_EQ(resource_attr(rm, "host.name"), "srv-web-01");
  EXPECT_FALSE(has_resource_attr(rm, "host.id"));
  EXPECT_FALSE(has_resource_attr(rm, "os.type"));
}

/* Resource attributes are frozen for a host within a batch... */
TEST_F(RequestBuilderTest, metadata_change_applies_to_the_next_batch) {
  auto store = std::make_shared<host_metadata_store>(std::chrono::seconds(900));
  AgentHostInfo info = make_host_info("m1");
  store->update(info, std::time(nullptr));
  request_builder b(conf, enricher, mapping, logger, store);
  ASSERT_TRUE(b.add_service_status(make_status("rta=1ms")));

  info.set_os_version("9.5");
  info.set_observed_at(info.observed_at() + 1);
  ASSERT_EQ(store->update(info, std::time(nullptr)),
            host_metadata_store::update_result::updated);
  ASSERT_TRUE(b.add_service_status(make_status("rta=2ms")));
  auto req = b.take();
  ASSERT_EQ(req.resource_metrics_size(), 1);
  EXPECT_EQ(resource_attr(req.resource_metrics(0), "os.version"), "9.4");

  ASSERT_TRUE(b.add_service_status(make_status("rta=3ms")));
  EXPECT_EQ(resource_attr(b.peek().resource_metrics(0), "os.version"), "9.5");
}

/* ...except when the machine changes: later samples must not be attributed to
 * the former machine. */
TEST_F(RequestBuilderTest, identity_change_starts_a_new_resource) {
  auto store = std::make_shared<host_metadata_store>(std::chrono::seconds(900));
  AgentHostInfo info = make_host_info("m1");
  store->update(info, std::time(nullptr));
  request_builder b(conf, enricher, mapping, logger, store);
  ASSERT_TRUE(b.add_service_status(make_status("rta=1ms")));

  info.set_machine_id("m2");
  info.set_observed_at(info.observed_at() + 1);
  ASSERT_EQ(store->update(info, std::time(nullptr)),
            host_metadata_store::update_result::identity_changed);
  b.start_new_resource(42);
  ASSERT_TRUE(b.add_service_status(make_status("rta=2ms")));

  const auto& req = b.peek();
  ASSERT_EQ(req.resource_metrics_size(), 2);
  EXPECT_EQ(resource_attr(req.resource_metrics(0), "host.id"), "m1");
  EXPECT_EQ(resource_attr(req.resource_metrics(1), "host.id"), "m2");
  EXPECT_EQ(req.resource_metrics(0).scope_metrics(0).metrics_size(),
            req.resource_metrics(1).scope_metrics(0).metrics_size());
}
