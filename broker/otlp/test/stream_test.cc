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

#include "com/centreon/broker/otlp/stream.hh"

#include <gtest/gtest.h>

#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::otlp;
using log_v2 = com::centreon::common::log_v2::log_v2;

namespace {

class fake_enricher : public resource_enricher {
 public:
  bool resolve = true;
  std::optional<std::string> host_name(uint64_t host_id) override {
    if (!resolve)
      return std::nullopt;
    return "host-" + std::to_string(host_id);
  }
  std::optional<std::string> service_description(uint64_t,
                                                 uint64_t s) override {
    return "svc-" + std::to_string(s);
  }
};

/* Records exports instead of performing them; lets the test decide when (and
 * whether) each one completes. */
class fake_exporter : public exporter_base {
 public:
  struct call {
    ExportRequest request;
    uint64_t nb_data;
    export_callback cb;
  };
  std::vector<call> calls;
  bool auto_complete = true;

  void export_async(ExportRequest&& request,
                    uint64_t nb_data,
                    export_callback cb) override {
    calls.push_back({std::move(request), nb_data, cb});
    if (auto_complete)
      cb(::grpc::Status::OK, ExportResponse{}, nb_data);
  }

  void complete_all() {
    for (auto& c : calls)
      c.cb(::grpc::Status::OK, ExportResponse{}, c.nb_data);
  }
};

class StreamTest : public ::testing::Test {
 public:
  std::shared_ptr<fake_enricher> enricher;
  std::shared_ptr<fake_exporter> exporter;
  otlp_config::pointer conf;
  std::shared_ptr<spdlog::logger> logger;

  void SetUp() override {
    enricher = std::make_shared<fake_enricher>();
    exporter = std::make_shared<fake_exporter>();
    conf = std::make_shared<otlp_config>();
    logger = log_v2::instance().get(log_v2::OTL);
  }

  std::unique_ptr<stream> make_stream() {
    return std::make_unique<stream>(conf, enricher, exporter, logger);
  }

  static std::shared_ptr<io::data> service_event(uint64_t host_id,
                                                 uint64_t service_id,
                                                 const std::string& perf) {
    auto e = std::make_shared<neb::pb_service_status>();
    auto& o = e->mut_obj();
    o.set_host_id(host_id);
    o.set_service_id(service_id);
    o.set_last_check(1700000000);
    o.set_state(ServiceStatus::OK);
    o.set_state_type(ServiceStatus::HARD);
    o.set_perfdata(perf);
    return e;
  }
};

}  // namespace

TEST_F(StreamTest, read_is_not_supported) {
  auto s = make_stream();
  std::shared_ptr<io::data> d;
  EXPECT_THROW(s->read(d, 0), com::centreon::broker::exceptions::shutdown);
}

/* write() reports events delivered since the last call; the muxer pops that
 * many. Every event must be accounted for or the pipeline stalls. */
TEST_F(StreamTest, each_write_acknowledges_its_event) {
  auto s = make_stream();
  EXPECT_EQ(s->write(service_event(1, 1, "rta=250ms")), 1);
  EXPECT_EQ(s->write(service_event(1, 2, "rta=250ms")), 1);
}

/* Events this module does not consume must still be acknowledged, otherwise
 * they would block the muxer forever. */
TEST_F(StreamTest, unhandled_event_is_acknowledged) {
  auto s = make_stream();
  auto other = std::make_shared<neb::pb_instance>();
  EXPECT_EQ(s->write(other), 1);
}

/* A host whose name cannot be resolved is still acknowledged: it will never
 * become resolvable by waiting, so holding it would stall the pipeline. */
TEST_F(StreamTest, unresolvable_host_is_still_acknowledged) {
  enricher->resolve = false;
  auto s = make_stream();
  EXPECT_EQ(s->write(service_event(1, 1, "rta=250ms")), 1);

  nlohmann::json tree;
  s->statistics(tree);
  EXPECT_EQ(tree["dropped_no_host_name"], 1);
  EXPECT_TRUE(exporter->calls.empty());
}

TEST_F(StreamTest, batch_is_sent_when_full) {
  conf->max_datapoints_per_batch = 3;
  auto s = make_stream();

  /* Each status yields a value plus a state datapoint. */
  s->write(service_event(1, 1, "rta=250ms"));
  EXPECT_TRUE(exporter->calls.empty()) << "2 datapoints is below the limit";

  s->write(service_event(1, 2, "rta=250ms"));
  EXPECT_EQ(exporter->calls.size(), 1u) << "4 datapoints crosses the limit";
}

TEST_F(StreamTest, stop_flushes_the_pending_batch) {
  auto s = make_stream();
  s->write(service_event(1, 1, "rta=250ms"));
  EXPECT_TRUE(exporter->calls.empty());

  s->stop();
  EXPECT_EQ(exporter->calls.size(), 1u);
}

TEST_F(StreamTest, flush_respects_the_send_interval) {
  auto s = make_stream();
  s->write(service_event(1, 1, "rta=250ms"));

  /* The interval has not elapsed, so nothing should go out yet. */
  s->flush();
  EXPECT_TRUE(exporter->calls.empty());

  conf->max_send_interval = 0;
  s->flush();
  EXPECT_EQ(exporter->calls.size(), 1u);
}

/* When exports are saturated the module must stop sending rather than grow
 * its own queue: retention belongs to the muxer and its splitter file. */
TEST_F(StreamTest, saturated_exporter_defers_instead_of_queueing) {
  conf->max_inflight_requests = 1;
  conf->max_datapoints_per_batch = 1;
  exporter->auto_complete = false;
  auto s = make_stream();

  s->write(service_event(1, 1, "rta=250ms"));
  EXPECT_EQ(exporter->calls.size(), 1u);

  s->write(service_event(1, 2, "rta=250ms"));
  EXPECT_EQ(exporter->calls.size(), 1u) << "must not exceed max_inflight";

  exporter->complete_all();
  s->write(service_event(1, 3, "rta=250ms"));
  EXPECT_EQ(exporter->calls.size(), 2u) << "resumes once a slot frees";
}

TEST_F(StreamTest, statistics_report_delivery) {
  auto s = make_stream();
  s->write(service_event(1, 1, "rta=250ms"));
  s->stop();

  nlohmann::json tree;
  s->statistics(tree);
  EXPECT_EQ(tree["batches_sent"], 1);
  EXPECT_GT(tree["datapoints_sent"], 0);
  EXPECT_EQ(tree["export_errors"], 0);
  EXPECT_EQ(tree["inflight_requests"], 0);
}
