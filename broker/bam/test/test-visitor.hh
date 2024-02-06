/*
** Copyright 2021 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_TEST_VISITOR_HH
#define CCB_TEST_VISITOR_HH

#include <fmt/format.h>
#include "bbdo/bam/ba_event.hh"
#include "bbdo/bam/inherited_downtime.hh"
#include "bbdo/bam/kpi_event.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/io/stream.hh"

namespace com::centreon::broker {
class test_visitor : public io::stream {
 public:
  class test_event {
   public:
    enum etype { kpi, ba, idt };
    etype typ;
    time_t start_time;
    time_t end_time;
    uint32_t kpi_id;
    uint32_t ba_id;
    int status;
    bool in_downtime;
    test_event(const bam::kpi_event& k)
        : typ{test_event::kpi},
          start_time{k.start_time},
          end_time{k.end_time},
          kpi_id{k.kpi_id},
          ba_id{k.ba_id},
          status{k.status},
          in_downtime{k.in_downtime} {}
    test_event(const bam::pb_kpi_event& k)
        : typ{test_event::kpi},
          start_time{time_t(k.obj().start_time())},
          end_time{k.obj().end_time()},
          kpi_id{k.obj().kpi_id()},
          ba_id{k.obj().ba_id()},
          status{k.obj().status()},
          in_downtime{k.obj().in_downtime()} {}
    test_event(const bam::ba_event& b)
        : typ{test_event::ba},
          start_time{b.start_time},
          end_time{b.end_time},
          ba_id{b.ba_id},
          status{b.status},
          in_downtime{b.in_downtime} {}
    test_event(const bam::pb_ba_event& b)
        : typ{test_event::ba},
          start_time{(time_t)b.obj().start_time()},
          end_time{(time_t)b.obj().end_time()},
          ba_id{b.obj().ba_id()},
          status{b.obj().status()},
          in_downtime{b.obj().in_downtime()} {}
    test_event(const bam::inherited_downtime& idt)
        : typ{test_event::idt},
          ba_id{idt.ba_id},
          in_downtime{idt.in_downtime} {}
    test_event(const bam::pb_inherited_downtime& idt)
        : typ{test_event::idt},
          ba_id{idt.obj().ba_id()},
          in_downtime{idt.obj().in_downtime()} {}
  };

 private:
  std::deque<test_event> _queue;

 public:
  test_visitor(const std::string& name) : io::stream(name) {}

  int32_t stop() override { return 0; }

  bool read(std::shared_ptr<io::data>& d __attribute__((unused)),
            time_t deadline __attribute__((unused))) override {
    return true;
  }

  int32_t write(const std::shared_ptr<io::data>& d) override {
    /* We keep kpi_events */
    switch (d->type()) {
      case 393221:
        _queue.emplace_back(*std::static_pointer_cast<bam::kpi_event>(d));
        break;
      case bam::pb_kpi_event::static_type():
        _queue.emplace_back(*std::static_pointer_cast<bam::pb_kpi_event>(d));
        break;
      case 393220:
        _queue.emplace_back(*std::static_pointer_cast<bam::ba_event>(d));
        break;
      case bam::pb_ba_event::static_type():
        _queue.emplace_back(*std::static_pointer_cast<bam::pb_ba_event>(d));
        break;
      case bam::inherited_downtime::static_type():
        _queue.emplace_back(
            *std::static_pointer_cast<bam::inherited_downtime>(d));
        break;
      case bam::pb_inherited_downtime::static_type():
        _queue.emplace_back(
            *std::static_pointer_cast<bam::pb_inherited_downtime>(d));
        break;

      default:
        break;
    }
    return 1;
  }

  const std::deque<test_event>& queue() const { return _queue; }

  void clear() { _queue.clear(); }

  void print_events() const {
    for (auto& e : _queue) {
      switch (e.typ) {
        case test_visitor::test_event::kpi:
          std::cout << fmt::format(
              "[{};{}] : KPI_EVENT - kpi {} ba {} status {} downtime {}\n",
              e.start_time, e.end_time, e.kpi_id, e.ba_id, e.status,
              e.in_downtime);
          break;
        case test_visitor::test_event::ba:
          std::cout << fmt::format(
              "[{};{}] :  BA_EVENT - ba {} status {} downtime {}\n",
              e.start_time, e.end_time, e.ba_id, e.status, e.in_downtime);
          break;
        case test_visitor::test_event::idt:
          std::cout << fmt::format("====> INHERITED_DT - ba {} downtime {}\n",
                                   e.ba_id, e.in_downtime);
          break;
      }
    }
  }
};
}

#endif  // !CCB_TEST_VISITOR_HH
