/**
 * Copyright 2011-2013,2015,2017 Centreon
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

#include "com/centreon/broker/rrd/lib.hh"

#include <fcntl.h>
#include <rrd.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <absl/container/flat_hash_map.h>

#include "bbdo/storage/metric.hh"
#include "com/centreon/broker/rrd/exceptions/open.hh"
#include "com/centreon/broker/rrd/exceptions/update.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;

/**
 *  Constructor.
 *
 *  @param[in] tmpl_path  The template path.
 *  @param[in] cache_size The maximum number of cache element.
 */
lib::lib(std::filesystem::path tmpl_path, uint32_t cache_size)
    : _creator(std::move(tmpl_path), cache_size) {}

/**
 *  @brief Initiates the bulk load of multiple commands.
 *
 *  With the librrd backend, this method does nothing.
 */
void lib::begin() {}

/**
 *  Clean the template cache.
 */
void lib::clean() {
  _creator.clear();
}

/**
 *  Close the RRD file.
 */
void lib::close() {
  _filename.clear();
}

/**
 *  @brief Commit transaction started with begin().
 *
 *  With the librrd backend, the method does nothing.
 */
void lib::commit() {}

/**
 *  Open a RRD file which already exists.
 *
 *  @param[in] filename Path to the RRD file.
 */
void lib::open(std::string const& filename) {
  // Close previous file.
  this->close();

  // Check that the file exists.
  if (access(filename.c_str(), F_OK))
    throw exceptions::open("RRD: file '{}' does not exist", filename);

  // Remember information for further operations.
  _filename = filename;
}

/**
 *  Open a RRD file and create it if it does not exists.
 *
 *  @param[in] filename   Path to the RRD file.
 *  @param[in] length     Duration in seconds that the RRD file should
 *                        retain.
 *  @param[in] from       Timestamp of the first record.
 *  @param[in] step       Time interval between each record.
 *  @param[in] value_type Type of the metric.
 */
void lib::open(std::string const& filename,
               uint32_t length,
               time_t from,
               uint32_t step,
               short value_type,
               bool without_cache) {
  // Close previous file.
  this->close();

  // Remember informations for further operations.
  _filename = filename;
  _creator.create(filename, length, from, step, value_type, without_cache);
}

/**
 *  Remove the RRD file.
 *
 *  @param[in] filename Path to the RRD file.
 */
void lib::remove(std::string const& filename) {
  if (::remove(filename.c_str())) {
    char const* msg(strerror(errno));
    _logger->error("RRD: could not remove file '{}': {}", filename, msg);
  } else
    SPDLOG_LOGGER_INFO(_logger, "remove file {}", filename);
}

/**
 *  Update the RRD file with new value.
 *
 *  @param[in] t     Timestamp of value.
 *  @param[in] value Associated value.
 */
void lib::update(time_t t, std::string const& value) {
  // Build argument string.
  if (value == "") {
    _logger->error("RRD: ignored update non-float value '{}' in file '{}'",
                   value, _filename);
    return;
  }

  std::string arg(fmt::format("{}:{}", t, value));

  // Set argument table.
  char const* argv[2];
  argv[0] = arg.c_str();
  argv[1] = nullptr;

  // Debug message.
  _logger->debug("RRD: updating file '{}' ({})", _filename, argv[0]);

  // Update RRD file.
  rrd_clear_error();
  if (rrd_update_r(_filename.c_str(), nullptr, sizeof(argv) / sizeof(*argv) - 1,
                   argv)) {
    char const* msg(rrd_get_error());
    if (!strstr(msg, "illegal attempt to update using time"))
      _logger->error("RRD: failed to update value in file '{}': {}", _filename,
                     msg);

    else
      _logger->error("RRD: ignored update error in file '{}': {}", _filename,
                     msg);
  }
}

void lib::update(const std::deque<std::string>& pts) {
  const char* argv[pts.size() + 1];
  argv[pts.size()] = nullptr;
  auto it = pts.begin();
  for (uint32_t i = 0; i < pts.size(); i++) {
    _logger->trace("insertion of {} in rrd file", *it);
    argv[i] = it->data();
    ++it;
  }
  rrd_clear_error();
  if (rrd_update_r(_filename.c_str(), nullptr, sizeof(argv) / sizeof(*argv) - 1,
                   argv)) {
    char const* msg(rrd_get_error());
    if (!strstr(msg, "illegal attempt to update using time"))
      _logger->error("RRD: failed to update value in file '{}': {}", _filename,
                     msg);

    else
      _logger->error("RRD: ignored update error in file '{}': {}", _filename,
                     msg);
  }
}

/**
 * @brief Read RRD file metadata and fetch all known data-points in
 *        [@p from_ts, @p to_ts] using librrd directly.
 *
 * Uses rrd_info_r() to determine step, rrd_len and value_type, then
 * rrd_fetch_r() to retrieve the actual data.  NaN values (unknown) are
 * omitted from the returned vector.
 *
 * @return rrd_existing_data with step == 0 on any error.
 */
rrd_existing_data lib::fetch_existing(const std::string& filename,
                                      uint64_t from_ts,
                                      uint64_t to_ts) {
  rrd_existing_data result;

  // ---- 1. Parse file metadata via rrd_info_r ----
  rrd_info_t* info = rrd_info_r(const_cast<char*>(filename.c_str()));
  if (!info) {
    _logger->error("RRD: rrd_info_r failed for '{}': {}", filename,
                   rrd_get_error());
    return result;
  }

  // Per-RRA data needed to compute max retention.
  absl::flat_hash_map<int, uint32_t> rra_pdp_per_row;
  absl::flat_hash_map<int, uint32_t> rra_rows;

  for (rrd_info_t* it = info; it; it = it->next) {
    std::string_view key{it->key};

    if (key == "step" && it->type == RD_I_CNT) {
      result.step = static_cast<uint32_t>(it->value.u_cnt);
    } else if (key == "ds[value].type" && it->type == RD_I_STR) {
      std::string_view type{it->value.u_str};
      if (type == "COUNTER")
        result.value_type = 1;
      else if (type == "DERIVE")
        result.value_type = 2;
      else if (type == "ABSOLUTE")
        result.value_type = 3;
      // else GAUGE = 0 (default)
    } else {
      int rra_idx = -1;
      if (std::sscanf(it->key, "rra[%d].pdp_per_row", &rra_idx) == 1 &&
          it->type == RD_I_CNT) {
        rra_pdp_per_row[rra_idx] = static_cast<uint32_t>(it->value.u_cnt);
      } else if (std::sscanf(it->key, "rra[%d].rows", &rra_idx) == 1 &&
                 it->type == RD_I_CNT) {
        rra_rows[rra_idx] = static_cast<uint32_t>(it->value.u_cnt);
      }
    }
  }
  rrd_info_free(info);

  if (result.step == 0) {
    _logger->error("RRD: could not read step from '{}'", filename);
    return result;
  }

  // Compute max retention across all RRAs.
  for (auto& [idx, pdp] : rra_pdp_per_row) {
    auto rows_it = rra_rows.find(idx);
    if (rows_it != rra_rows.end()) {
      uint32_t retention = result.step * pdp * rows_it->second;
      result.rrd_len = std::max(result.rrd_len, retention);
    }
  }

  // ---- 2. Fetch data via rrd_fetch_r ----
  time_t start = static_cast<time_t>(from_ts) - static_cast<time_t>(result.step);
  time_t end = static_cast<time_t>(to_ts);
  unsigned long step_out = 0, ds_cnt = 0;
  char** ds_names = nullptr;
  rrd_value_t* data = nullptr;

  rrd_clear_error();
  if (rrd_fetch_r(filename.c_str(), "AVERAGE", &start, &end, &step_out,
                  &ds_cnt, &ds_names, &data) != 0) {
    _logger->warn("RRD: rrd_fetch_r failed for '{}': {}", filename,
                  rrd_get_error());
    return result;
  }

  if (step_out > 0 && ds_cnt > 0 && data) {
    size_t n = static_cast<size_t>((end - start) / static_cast<time_t>(step_out));
    uint64_t t = static_cast<uint64_t>(start) + step_out;  // first timestamp
    for (size_t i = 0; i < n; ++i, t += step_out) {
      double v = data[i * ds_cnt];  // DS index 0
      if (!std::isnan(v))
        result.points.emplace_back(t, v);
    }
  }

  // Free rrd_fetch_r allocations.
  free(data);
  if (ds_names) {
    for (unsigned long i = 0; i < ds_cnt; ++i)
      free(ds_names[i]);
    free(ds_names);
  }

  return result;
}

/**
 * @brief Create a new RRD file at @p tmp_path via librrd and write @p batch
 *        into it.  Bypasses rrdcached entirely (safe during merge).
 */
void lib::merge_create_temp(const std::string& tmp_path,
                            uint32_t rrd_len,
                            time_t from,
                            uint32_t step,
                            short value_type,
                            const std::deque<std::string>& batch) {
  // Create (or overwrite) the temp file.
  lib::open(tmp_path, rrd_len, from, step, value_type, /*without_cache=*/true);
  // Write all merged points using librrd directly.
  lib::update(batch);
}
