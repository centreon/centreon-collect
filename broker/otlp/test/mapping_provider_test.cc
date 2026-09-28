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

#include "com/centreon/broker/otlp/mapping_provider.hh"

#include <gtest/gtest.h>
#include <fstream>
#include <thread>

#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker::otlp;
using com::centreon::common::perfdata;
using com::centreon::exceptions::msg_fmt;
using log_v2 = com::centreon::common::log_v2::log_v2;

extern std::shared_ptr<asio::io_context> g_io_context;

namespace {

constexpr std::string_view k_one_rule = R"({
  "metrics": {
    "my.queue.depth": {
      "name": "messaging.queue.depth",
      "unit": "{message}",
      "instrument": "sum_non_monotonic",
      "scale": 2,
      "attributes": { "messaging.system": "rabbitmq" },
      "instance_attribute": "messaging.destination.name"
    }
  }
})";

void write_file(const std::filesystem::path& path, std::string_view content) {
  std::ofstream f(path, std::ios::trunc);
  f << content;
}

}  // namespace

/* ------------------------------------------------------------------ */
/* JSON parsing                                                        */
/* ------------------------------------------------------------------ */

TEST(otlp_mapping_table, empty_table_has_no_rule) {
  ASSERT_TRUE(mapping_table::empty());
  EXPECT_EQ(mapping_table::empty()->size(), 0u);
  EXPECT_EQ(mapping_table::empty()->find("cpu.user.percentage"), nullptr);
}

/* Without a mapping file, everything is exported under centreon.*. */
TEST(otlp_mapping_table, empty_table_maps_everything_to_centreon) {
  auto m = map_metric("/var#disk.space.usage.bytes", "B", perfdata::gauge,
                      *mapping_table::empty());
  EXPECT_TRUE(m.is_fallback);
  EXPECT_EQ(m.name, "centreon.disk.space.usage.bytes");
  EXPECT_EQ(m.unit, "By");
  EXPECT_DOUBLE_EQ(m.scale, 1.0);
  ASSERT_EQ(m.attributes.size(), 1u);
  EXPECT_EQ(m.attributes[0].first, "centreon.metric.instance");
  EXPECT_EQ(m.attributes[0].second, "/var");
}

TEST(otlp_mapping_table, every_field_is_parsed) {
  auto table = mapping_table::from_json(k_one_rule);
  ASSERT_EQ(table->size(), 1u);
  const mapping_rule* r = table->find("my.queue.depth");
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->name, "messaging.queue.depth");
  EXPECT_EQ(r->unit, "{message}");
  EXPECT_EQ(r->instr, instrument::sum_non_monotonic);
  EXPECT_DOUBLE_EQ(r->scale, 2.0);
  ASSERT_EQ(r->attributes.size(), 1u);
  EXPECT_EQ(r->attributes[0].first, "messaging.system");
  EXPECT_EQ(r->attributes[0].second, "rabbitmq");
  EXPECT_EQ(r->instance_attribute, "messaging.destination.name");
}

TEST(otlp_mapping_table, optional_fields_have_defaults) {
  auto table = mapping_table::from_json(
      R"({"metrics": {"foo": {"name": "bar"}}})");
  const mapping_rule* r = table->find("foo");
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->unit, "");
  EXPECT_EQ(r->instr, instrument::gauge);
  EXPECT_DOUBLE_EQ(r->scale, 1.0);
  EXPECT_TRUE(r->attributes.empty());
  EXPECT_TRUE(r->instance_attribute.empty());
}

TEST(otlp_mapping_table, invalid_documents_are_rejected) {
  EXPECT_THROW(mapping_table::from_json("{ not json"), msg_fmt);
  EXPECT_THROW(mapping_table::from_json(R"({})"), msg_fmt);
  /* rule without a name */
  EXPECT_THROW(mapping_table::from_json(R"({"metrics": {"foo": {}}})"),
               msg_fmt);
  EXPECT_THROW(mapping_table::from_json(
                   R"({"metrics": {"foo": {"name": "x", "instrument": "hist"}}})"),
               msg_fmt);
  /* a misspelled key must not be silently ignored */
  EXPECT_THROW(mapping_table::from_json(
                   R"({"metrics": {"foo": {"name": "x", "scal": 0.01}}})"),
               msg_fmt);
  EXPECT_THROW(
      mapping_table::from_json(
          R"({"metrics": {"foo": {"name": "x", "attributes": {"k": 1}}}})"),
      msg_fmt);
}

TEST(otlp_mapping_table, map_metric_uses_the_given_table) {
  auto table = mapping_table::from_json(k_one_rule);
  auto m = map_metric("orders#my.queue.depth", "", perfdata::gauge, *table);
  EXPECT_FALSE(m.is_fallback);
  EXPECT_EQ(m.name, "messaging.queue.depth");
  EXPECT_EQ(m.instr, instrument::sum_non_monotonic);
  EXPECT_DOUBLE_EQ(m.scale, 2.0);
  ASSERT_EQ(m.attributes.size(), 2u);
  EXPECT_EQ(m.attributes[1].first, "messaging.destination.name");
  EXPECT_EQ(m.attributes[1].second, "orders");

  /* a label absent from the table falls back to centreon.* */
  EXPECT_TRUE(
      map_metric("cpu.user.percentage", "%", perfdata::gauge, *table)
          .is_fallback);
}

TEST(otlp_mapping_table, missing_instance_falls_back) {
  auto table = mapping_table::from_json(k_one_rule);
  auto m = map_metric("my.queue.depth", "", perfdata::gauge, *table);
  EXPECT_TRUE(m.is_fallback);
  EXPECT_EQ(m.name, "centreon.my.queue.depth");
}

/* ------------------------------------------------------------------ */
/* File loading and hot reload                                         */
/* ------------------------------------------------------------------ */

class MappingProviderTest : public ::testing::Test {
 public:
  std::filesystem::path path;
  std::shared_ptr<spdlog::logger> logger;

  void SetUp() override {
    logger = log_v2::instance().get(log_v2::OTL);
    path = std::filesystem::temp_directory_path() /
           fmt::format("otlp_mapping_test_{}.json", getpid());
    write_file(path, k_one_rule);
  }

  void TearDown() override { std::filesystem::remove(path); }

  static bool wait_for(const std::function<bool()>& cond) {
    auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < limit) {
      if (cond())
        return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return cond();
  }
};

TEST_F(MappingProviderTest, empty_provider_serves_empty_table) {
  auto p = mapping_provider::empty(logger);
  EXPECT_EQ(p->get(), mapping_table::empty());
  EXPECT_FALSE(p->reload());
}

TEST_F(MappingProviderTest, missing_file_fails_at_load) {
  EXPECT_THROW(mapping_provider::load(g_io_context, path.string() + ".nope",
                                      logger),
               msg_fmt);
}

TEST_F(MappingProviderTest, invalid_file_fails_at_load) {
  write_file(path, R"({"metrics": {"foo": {}}})");
  EXPECT_THROW(mapping_provider::load(g_io_context, path, logger), msg_fmt);
}

TEST_F(MappingProviderTest, invalid_rewrite_keeps_previous_table) {
  auto p = mapping_provider::load(g_io_context, path, logger);
  auto before = p->get();
  write_file(path, "{ broken");
  EXPECT_FALSE(p->reload());
  EXPECT_EQ(p->get(), before);
}

/* A snapshot taken before a reload stays usable after it. */
TEST_F(MappingProviderTest, snapshot_survives_reload) {
  auto p = mapping_provider::load(g_io_context, path, logger);
  auto snapshot = p->get();
  write_file(path, R"({"metrics": {}})");
  ASSERT_TRUE(p->reload());
  EXPECT_EQ(p->get()->size(), 0u);
  EXPECT_NE(snapshot->find("my.queue.depth"), nullptr);
}

TEST_F(MappingProviderTest, file_rewrite_is_picked_up_by_the_watcher) {
  auto p = mapping_provider::load(g_io_context, path, logger);
  ASSERT_NE(p->get()->find("my.queue.depth"), nullptr);
  /* let the watch be established on the io_context thread */
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  write_file(path, R"({"metrics": {"other": {"name": "x.y"}}})");
  EXPECT_TRUE(
      wait_for([&] { return p->get()->find("other") != nullptr; }));
  EXPECT_EQ(p->get()->find("my.queue.depth"), nullptr);
}
