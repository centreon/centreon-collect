/**
 * Copyright 2011-2017-2024 Centreon
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

#include "com/centreon/broker/file/stream.hh"

#include <fmt/chrono.h>

#include "broker.pb.h"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/misc/math.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::file;

using log_v2 = com::centreon::common::log_v2::log_v2;

static constexpr double eps = 0.000001;

/**
 *  Constructor.
 *
 *  @param[in] file  Splitted file on which the stream will operate.
 */
stream::stream(const std::string& path,
               QueueFileStats* s,
               uint32_t max_file_size,
               bool auto_delete)
    : io::stream("file"),
      _splitter(path, max_file_size, auto_delete),
      _stats{s},
      _last_stats{time(nullptr)},
      _last_stats_perc{time(nullptr)},
      _last_read_offset(0),
      _last_time(0),
      _last_write_offset(0),
      _stats_perc{},
      _stats_idx{0u},
      _stats_size{0u},
      _center{config::applier::state::instance().center()} {}

/**
 *  Get peer name.
 *
 *  @return Peer name.
 */
std::string stream::peer() const {
  return fmt::format("file://{}", _splitter.get_file_path());
}

/**
 *  Read data from the file.
 *
 *  @param[out] d         Bunch of data.
 *  @param[in]  deadline  Timeout.
 *
 *  @return Always true as file never times out.
 */
bool stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
  (void)deadline;

  d.reset();

  // Build data array.
  std::unique_ptr<io::raw> data(new io::raw);
  data->resize(BUFSIZ);

  // Read data.
  long rb(_splitter.read(data->data(), data->size()));
  if (rb) {
    data->resize(rb);
    d.reset(data.release());
  }

  _update_stats();
  return true;
}

/**
 *  Generate statistics about file processing.
 *
 *  @param[out] buffer Output buffer.
 */
void stream::statistics(nlohmann::json& tree) const {
  // Get base properties.
  uint32_t max_file_size(_splitter.max_file_size());
  int rid(_splitter.get_rid());
  long roffset(_splitter.get_roffset());
  int wid(_splitter.get_wid());
  long woffset(_splitter.get_woffset());

  // Easy to print.
  tree["file_read_path"] = rid;
  tree["file_read_offset"] = static_cast<double>(roffset);
  tree["file_write_path"] = wid;
  tree["file_write_offset"] = static_cast<double>(woffset);
  if (max_file_size != std::numeric_limits<uint32_t>::max())
    tree["file_max_size"] = static_cast<double>(max_file_size);
  else
    tree["file_max_size"] = "unlimited";

  // Need computation.
  bool write_time_expected(false);
  long long froffset(roffset + rid * static_cast<long long>(max_file_size));
  long long fwoffset(woffset + wid * static_cast<long long>(max_file_size));
  if ((rid != wid && max_file_size == std::numeric_limits<uint32_t>::max()) ||
      !fwoffset) {
    tree["file_percent_processed"] = "unknown";
  } else {
    tree["file_percent_processed"] = 100.0 * froffset / fwoffset;
    write_time_expected = true;
  }
  if (write_time_expected) {
    time_t now(time(nullptr));

    if (_last_time && now != _last_time) {
      time_t eta(0);
      {
        unsigned long long div(froffset + _last_write_offset -
                               _last_read_offset - fwoffset);
        if (div == 0)
          tree["file_expected_terminated_at"] =
              "file not processed fast enough to terminate";
        else {
          eta = now + (fwoffset - froffset) * (now - _last_time) / div;
          tree["file_expected_terminated_at"] = static_cast<double>(eta);
        }
      }

      if (max_file_size == std::numeric_limits<uint32_t>::max()) {
        tree["file_expected_max_size"] = static_cast<double>(
            fwoffset +
            (fwoffset - _last_write_offset) * (eta - now) / (now - _last_time));
      }
    }

    _last_time = now;
    _last_read_offset = froffset;
    _last_write_offset = fwoffset;
  }
}

/**
 * @brief Update persistent file statistics stored in _stats.
 */
void stream::_update_stats() {
  if (_stats == nullptr)
    return;

  time_t now = time(nullptr);
  if (now > _last_stats) {
    _last_stats = now;

    const double mm = _splitter.max_file_size();
    int32_t roffset = _splitter.get_roffset();
    int32_t woffset = _splitter.get_woffset();
    int32_t wid = _splitter.get_wid();
    int32_t rid = _splitter.get_rid();
    double a = static_cast<double>(roffset) + static_cast<double>(rid) * mm;
    double b = static_cast<double>(woffset) + static_cast<double>(wid) * mm;
    double m, p;

    if (std::abs(b) >= eps) {
      double perc = 100.0 * a / b;
      double advance = b - a;
      bool reg = false;
      /* The regression is computed only every 5 seconds. */
      if (now > _last_stats_perc + 5) {
        _last_stats_perc = now;
        /* We store advance instead of perc, the first one is more linear
         * compared to the second that is hyperbolic. To obtain a linear
         * regression, the first one will give better results. */
        _stats_perc[_stats_idx] = std::make_pair(now, advance);
        ++_stats_idx;
        if (_stats_idx >= _stats_perc.size())
          _stats_idx = 0;
        if (_stats_size < _stats_perc.size())
          _stats_size++;

        reg = misc::least_squares(_stats_perc, _stats_size, m, p);
      }

      if (reg) {
        _center->execute(
            [s = this->_stats, now, wid, woffset, rid, roffset, perc, m, p] {
              s->set_file_write_path(wid);
              s->set_file_write_offset(woffset);
              s->set_file_read_path(rid);
              s->set_file_read_offset(roffset);
              s->set_file_percent_processed(perc);
              if (m != 0) {
                time_t terminated = static_cast<time_t>(-p / m);
                if (now > terminated) {
                  s->set_file_expected_terminated_at(
                      std::numeric_limits<int64_t>::max());
                  s->set_file_expected_terminated_in("too slow");
                } else {
                  s->set_file_expected_terminated_at(terminated);
                  int32_t duration = terminated - now;
                  int32_t sec = duration % 60;
                  duration /= 60;
                  int min = duration % 60;
                  duration /= 60;
                  std::string d;
                  if (duration)
                    d = fmt::format("{}h{}m{}s", duration, min, sec);
                  else if (min)
                    d = fmt::format("{}m{}s", min, sec);
                  else
                    d = fmt::format("{}s", sec);
                  s->set_file_expected_terminated_in(d);

                  log_v2::instance()
                      .get(log_v2::CORE)
                      ->info(
                          "Retention file will be terminated at {:%Y-%m-%d "
                          "%H:%M:%S}",
                          fmt::localtime(terminated));
                }
              } else
                s->set_file_expected_terminated_at(
                    std::numeric_limits<int64_t>::max());
            });
      } else {
        _center->execute([s = this->_stats, wid, woffset, rid, roffset, perc] {
          s->set_file_write_path(wid);
          s->set_file_write_offset(woffset);
          s->set_file_read_path(rid);
          s->set_file_read_offset(roffset);
          s->set_file_percent_processed(perc);
        });
      }
    }
  }
}

/**
 *  Write data to the file.
 *
 *  @param[in] d  Data to write.
 *
 *  @return Number of events acknowledged (1).
 */
int32_t stream::write(std::shared_ptr<io::data> const& d) {
  // Check that data exists.
  if (!validate(d, get_name()))
    return 1;

  if (d->type() == io::raw::static_type()) {
    // Get data.
    char const* memory;
    uint32_t size;
    {
      io::raw* data(static_cast<io::raw*>(d.get()));
      memory = data->data();
      size = data->size();
    }

    // Write data.
    while (size > 0) {
      long wb(_splitter.write(memory, size));
      size -= wb;
      memory += wb;
    }
    _update_stats();
  }

  return 1;
}

/**
 * @brief Flush the stream and stop it.
 *
 * @return The number of acknowledged events.
 */
int32_t stream::stop() {
  return 0;
}

/**
 *  Remove all the files this stream in concerned by.
 */
void stream::remove_all_files() {
  _splitter.remove_all_files();
}

uint32_t stream::max_file_size() const {
  return _splitter.max_file_size();
}
