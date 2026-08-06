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

#include "com/centreon/broker/otlp/semconv_mapping.hh"

#include <gtest/gtest.h>

using namespace com::centreon::broker::otlp;
using com::centreon::common::perfdata;

/* ------------------------------------------------------------------ */
/* Structured name decomposition                                       */
/* ------------------------------------------------------------------ */

TEST(otlp_decompose, flat_name_has_no_instance) {
  auto d = decompose("load1");
  EXPECT_EQ(d.metric, "load1");
  EXPECT_TRUE(d.instance.empty());
  EXPECT_TRUE(d.subinstances.empty());
}

TEST(otlp_decompose, instance_and_metric) {
  auto d = decompose("/var#disk.space.usage.bytes");
  EXPECT_EQ(d.instance, "/var");
  EXPECT_EQ(d.metric, "disk.space.usage.bytes");
  EXPECT_TRUE(d.subinstances.empty());
}

TEST(otlp_decompose, subinstances) {
  auto d = decompose("sda~read#disk.io.read.bytes");
  EXPECT_EQ(d.instance, "sda");
  EXPECT_EQ(d.metric, "disk.io.read.bytes");
  ASSERT_EQ(d.subinstances.size(), 1u);
  EXPECT_EQ(d.subinstances[0], "read");
}

TEST(otlp_decompose, empty_instance_before_sharp) {
  auto d = decompose("#metric.name");
  EXPECT_TRUE(d.instance.empty());
  EXPECT_EQ(d.metric, "metric.name");
}

/* ------------------------------------------------------------------ */
/* Unit conversion — the highest-risk part of the table                */
/* ------------------------------------------------------------------ */

TEST(otlp_mapping, percent_becomes_ratio) {
  auto m = map_metric("cpu.utilization.percentage", "%", perfdata::gauge);
  EXPECT_EQ(m.name, "system.cpu.utilization");
  EXPECT_EQ(m.unit, "1");
  EXPECT_DOUBLE_EQ(m.scale, 0.01);
  EXPECT_FALSE(m.is_fallback);
  /* 87% must land as 0.87, not 87. */
  EXPECT_DOUBLE_EQ(87.0 * m.scale, 0.87);
}

TEST(otlp_mapping, milliseconds_become_seconds) {
  auto m = map_metric("rta", "ms", perfdata::gauge);
  EXPECT_EQ(m.name, "centreon.icmp.rtt");
  EXPECT_EQ(m.unit, "s");
  EXPECT_DOUBLE_EQ(250.0 * m.scale, 0.25);
}

TEST(otlp_mapping, bits_per_second_become_bytes_per_second) {
  auto m = map_metric("eth0#interface.traffic.in.bitspersecond", "b/s",
                      perfdata::gauge);
  /* Deliberately not system.network.io: that is a cumulative byte counter
   * while this is a rate, so mapping it there would break rate() queries. */
  EXPECT_EQ(m.name, "centreon.network.throughput");
  EXPECT_EQ(m.unit, "By/s");
  EXPECT_DOUBLE_EQ(800.0 * m.scale, 100.0);
}

TEST(otlp_mapping, bytes_are_not_rescaled) {
  auto m = map_metric("memory.usage.bytes", "B", perfdata::gauge);
  EXPECT_EQ(m.name, "system.memory.usage");
  EXPECT_EQ(m.unit, "By");
  EXPECT_DOUBLE_EQ(m.scale, 1.0);
}

/* ------------------------------------------------------------------ */
/* Attributes                                                          */
/* ------------------------------------------------------------------ */

TEST(otlp_mapping, cpu_mode_attribute_is_current_spelling) {
  auto m = map_metric("cpu.user.percentage", "%", perfdata::gauge);
  ASSERT_EQ(m.attributes.size(), 1u);
  /* Renamed from system.cpu.state; older material has the previous name. */
  EXPECT_EQ(m.attributes[0].first, "cpu.mode");
  EXPECT_EQ(m.attributes[0].second, "user");
}

TEST(otlp_mapping, mountpoint_comes_from_the_instance_part) {
  auto m = map_metric("/var#disk.space.usage.bytes", "B", perfdata::gauge);
  EXPECT_EQ(m.name, "system.filesystem.usage");
  EXPECT_FALSE(m.is_fallback);
  bool found = false;
  for (const auto& [k, v] : m.attributes)
    if (k == "system.filesystem.mountpoint") {
      EXPECT_EQ(v, "/var");
      found = true;
    }
  EXPECT_TRUE(found) << "mountpoint must be bound from the instance part";
}

TEST(otlp_mapping, interface_name_comes_from_the_instance_part) {
  auto m = map_metric("eth0#interface.packets.in.count", "c",
                      perfdata::counter);
  EXPECT_EQ(m.name, "system.network.packet.count");
  EXPECT_EQ(m.instr, instrument::sum_monotonic);
  bool found = false;
  for (const auto& [k, v] : m.attributes)
    if (k == "network.interface.name") {
      EXPECT_EQ(v, "eth0");
      found = true;
    }
  EXPECT_TRUE(found);
}

/* A semconv metric identified by mountpoint is meaningless without one: every
 * filesystem would silently collapse into a single series. */
TEST(otlp_mapping, missing_required_instance_degrades_to_fallback) {
  auto m = map_metric("disk.space.usage.bytes", "B", perfdata::gauge);
  EXPECT_TRUE(m.is_fallback);
  EXPECT_EQ(m.name, "centreon.disk.space.usage.bytes");
  EXPECT_NE(m.name, "system.filesystem.usage");
}

/* ------------------------------------------------------------------ */
/* Fallback                                                            */
/* ------------------------------------------------------------------ */

TEST(otlp_mapping, unknown_metric_falls_back_to_centreon_namespace) {
  auto m = map_metric("my_custom_check", "queries", perfdata::gauge);
  EXPECT_TRUE(m.is_fallback);
  EXPECT_EQ(m.name, "centreon.my_custom_check");
  EXPECT_DOUBLE_EQ(m.scale, 1.0) << "fallback must not rescale";
  EXPECT_EQ(m.unit, "queries");
}

TEST(otlp_mapping, fallback_keeps_instance_as_attribute) {
  auto m = map_metric("queue1#custom.depth", "", perfdata::gauge);
  EXPECT_TRUE(m.is_fallback);
  EXPECT_EQ(m.name, "centreon.custom.depth");
  EXPECT_EQ(m.unit, "1");
  bool found = false;
  for (const auto& [k, v] : m.attributes)
    if (k == "centreon.metric.instance") {
      EXPECT_EQ(v, "queue1");
      found = true;
    }
  EXPECT_TRUE(found);
}

TEST(otlp_mapping, counter_fallback_is_a_monotonic_sum) {
  auto m = map_metric("weird_counter", "c", perfdata::counter);
  EXPECT_EQ(m.instr, instrument::sum_monotonic);
  auto g = map_metric("weird_gauge", "", perfdata::gauge);
  EXPECT_EQ(g.instr, instrument::gauge);
}

TEST(otlp_sanitize, strips_illegal_characters) {
  EXPECT_EQ(sanitize("Weird Name!"), "weird_name");
  EXPECT_EQ(sanitize("a.b.c"), "a.b.c");
  EXPECT_EQ(sanitize("  "), "unnamed");
  EXPECT_EQ(sanitize("UPPER"), "upper");
}

/* ------------------------------------------------------------------ */
/* Companion metric naming                                             */
/* ------------------------------------------------------------------ */

TEST(otlp_threshold_name, prefixes_semconv_names) {
  EXPECT_EQ(threshold_metric_name("system.filesystem.utilization"),
            "centreon.system.filesystem.utilization.threshold");
}

/* The prefix must not be doubled for metrics already in our namespace. */
TEST(otlp_threshold_name, does_not_repeat_centreon_prefix) {
  EXPECT_EQ(threshold_metric_name("centreon.icmp.rtt"),
            "centreon.icmp.rtt.threshold");
  EXPECT_EQ(bound_metric_name("centreon.icmp.rtt"), "centreon.icmp.rtt.bound");
}
