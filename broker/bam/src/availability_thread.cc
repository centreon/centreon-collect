/**
 * Copyright 2014, 2021-2024 Centreon
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

#include "com/centreon/broker/bam/availability_thread.hh"

#include <algorithm>
#include <chrono>

#include "com/centreon/broker/misc/time.hh"
#include "com/centreon/broker/sql/mysql_error.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;
using com::centreon::common::timeperiods::add_round_days_to_midnight;

/**
 *  Constructor.
 *
 *  @param[in] db_cfg       Database configuration.
 *  @param[in] shared_map   A timeperiod map shared with the reporting.
 *  @param[in] logger       The logger to use in availability_thread.
 */
availability_thread::availability_thread(
    database_config const& db_cfg,
    timeperiod_map& shared_map,
    const std::shared_ptr<spdlog::logger>& logger)
    : _started_flag{false},
      _db_cfg(db_cfg),
      _shared_tps(shared_map),
      _mutex{},
      _should_exit(false),
      _should_rebuild_all(false),
      _logger{logger} {}

/**
 *  Destructor.
 */
availability_thread::~availability_thread() {
  _close_database();
}

/**
 *  The main loop of thread.
 */
void availability_thread::run() {
  // Lock the mutex.
  std::unique_lock<std::mutex> lock(_mutex);

  // Check for termination asked.
  if (_should_exit)
    return;

  for (;;) {
    try {
      // Calculate the duration until next midnight.
      time_t midnight = _compute_next_midnight();
      unsigned long wait_for = std::difftime(midnight, ::time(nullptr));
      _logger->debug("BAM-BI: availability thread sleeping for {} seconds.",
                     wait_for);
      _wait.wait_for(lock, std::chrono::seconds(wait_for));
      _logger->debug("BAM-BI: availability thread waking up ");

      // Termination asked.
      if (_should_exit)
        break;

      _logger->debug("BAM-BI: opening database");
      // Open the database.
      _open_database();

      _logger->debug("BAM-BI: build availabilities");
      _build_availabilities(misc::start_of_day(::time(nullptr)));
      _should_rebuild_all = false;
      _bas_to_rebuild.clear();

      // Close the database.
      _close_database();
      _logger->debug("BAM-BI: database closed");
    } catch (const std::exception& e) {
      // Something bad happened. Wait for the next loop.
      _logger->error("BAM-BI: Something went wrong: {}", e.what());
      _close_database();
    }
  }
}

/**
 *  Ask for the thread termination.
 */
void availability_thread::terminate() {
  std::lock_guard<std::mutex> lock(_mutex);
  _should_exit = true;
  _wait.notify_one();
}

/**
 *  Start a thread, and wait for its initialization.
 */
void availability_thread::start_and_wait() {
  if (!_started_flag) {
    _thread = std::thread(&availability_thread::run, this);
    pthread_setname_np(_thread.native_handle(), "bam_avail_thrd");
    _started_flag = true;
  }
}

void availability_thread::wait() {
  _thread.join();
  _started_flag = false;
}

/**
 *  @brief Lock the main mutex of the availability thread.
 */
void availability_thread::lock() {
  _mutex.lock();
}

/**
 * @brief Unlock the main mutex of the availability thread.
 */
void availability_thread::unlock() {
  _mutex.unlock();
}

/**
 *  Ask the thread to rebuild the availabilities.
 *
 *  @param[in] bas_to_rebuild  A string containing the bas to rebuild.
 */
void availability_thread::rebuild_availabilities(
    std::string const& bas_to_rebuild) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (bas_to_rebuild.empty())
    return;
  _should_rebuild_all = true;
  _bas_to_rebuild = bas_to_rebuild;
  _wait.notify_one();
}

/**
 *  Delete all the availabilities.
 */
void availability_thread::_delete_all_availabilities() {
  _logger->debug("BAM-BI: availability thread deleting availabilities");

  // Prepare the query.
  std::string query_str(fmt::format(
      "DELETE FROM mod_bam_reporting_ba_availabilities WHERE ba_id IN ({})",
      _bas_to_rebuild));

  _mysql->run_query(query_str, database::mysql_error::delete_availabilities);
}

/**
 *  @brief  Build all the availabilities.
 *
 *  This is called from the context of the availability thread.
 *
 *  @param[in] mignight   Midnight of today.
 */
void availability_thread::_build_availabilities(time_t midnight) {
  time_t first_day = 0;
  time_t last_day = midnight;
  std::string query_str;
  int thread_id;

  // Get the first day of rebuilding. If a complete rebuilding was asked,
  // it's the day of the chronogically first event to rebuild.
  // If not, it's the day following the chronogically last availability.
  if (_should_rebuild_all) {
    query_str = fmt::format(
        "SELECT MIN(start_time), MAX(end_time), MIN(IFNULL(end_time, '0'))"
        " FROM mod_bam_reporting_ba_events  WHERE ba_id IN ({})",
        _bas_to_rebuild);
    try {
      std::promise<database::mysql_result> promise;
      std::future<database::mysql_result> future = promise.get_future();
      thread_id =
          _mysql->run_query_and_get_result(query_str, std::move(promise));
      database::mysql_result res(future.get());
      if (!_mysql->fetch_row(res))
        throw msg_fmt("no events matching BAs to rebuild");
      first_day = res.value_as_i32(0);
      first_day = misc::start_of_day(first_day);
      // If there is opened events, rebuild until midnight of this day.
      // If not, rebuild until the last closed events.
      if (res.value_as_i32(2) != 0)
        last_day = misc::start_of_day(res.value_as_i32(1));

      _delete_all_availabilities();
    } catch (const std::exception& e) {
      _logger->error(
          "BAM-BI: availability thread could not select the BA durations from "
          "the reporting database: {}",
          e.what());
      throw msg_fmt(
          "BAM-BI: availability thread could not select the BA durations "
          "from the reporting database: {}",
          e.what());
    }

  } else {
    query_str = "SELECT MAX(time_id) FROM mod_bam_reporting_ba_availabilities";
    try {
      std::promise<database::mysql_result> promise;
      std::future<database::mysql_result> future = promise.get_future();
      thread_id =
          _mysql->run_query_and_get_result(query_str, std::move(promise));
      database::mysql_result res(future.get());
      if (!_mysql->fetch_row(res)) {
        _logger->error("no availability in table");
        throw msg_fmt("no availability in table");
      }
      first_day = res.value_as_i32(0);
      first_day = add_round_days_to_midnight(first_day, 1);
    } catch (const std::exception& e) {
      std::string msg(fmt::format(
          "BAM-BI: availability thread could not select the BA availabilities "
          "from the reporting database: {}",
          e.what()));
      _logger->error(msg);
      throw msg_fmt(msg);
    }
  }

  _logger->debug(
      "BAM-BI: availability thread writing availabilities from: {} to {}",
      first_day, last_day);

  /* The availabilities are computed day by day, but read and written by
   * chunks of days: one query for the durations of the chunk, one for the
   * events still open, one multi-row INSERT for its availabilities. Until
   * 2026-09 every day cost two synchronous SELECTs and one INSERT per (BA,
   * period): a rebuild over years was thousands of round trips, all of them
   * under the lock the reporting stream also waits for. The chunk bounds the
   * memory a long history takes: a month of durations at a time. */
  constexpr uint32_t chunk_days = 31;
  const auto started = std::chrono::steady_clock::now();
  uint32_t days = 0;
  database::bulk_or_multi insert(
      "INSERT INTO mod_bam_reporting_ba_availabilities "
      "(ba_id, time_id, timeperiod_id, timeperiod_is_default,"
      " available, unavailable, degraded,"
      " unknown, downtime, alert_unavailable_opened,"
      " alert_degraded_opened, alert_unknown_opened,"
      " nb_downtime) VALUES ",
      "");
  while (first_day < last_day) {
    time_t chunk_end = add_round_days_to_midnight(first_day, chunk_days);
    if (chunk_end > last_day)
      chunk_end = last_day;
    _build_availabilities_chunk(thread_id, first_day, chunk_end, insert);
    for (time_t d = first_day; d < chunk_end;
         d = add_round_days_to_midnight(d, 1))
      ++days;
    first_day = chunk_end;
  }
  /* A barrier as much as a commit: the INSERTs are only queued on the
   * connection, and the caller may close it right after. */
  _mysql->commit(thread_id);
  _logger->info("BAM-BI: availabilities of {} days written in {} ms", days,
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
}

/**
 *  @brief  Build the availabilities of a range of days.
 *
 *  This is called from the context of the availability thread.
 *
 *  Two queries for the whole range: the durations that overlap it, sorted by
 *  start time, and the events still open. The days are then walked in memory:
 *  a duration takes part in every day between its start and its end, an open
 *  event in every day since its start. One builder per (BA, period, day)
 *  sums up what falls inside the day, and its row is added to the batch.
 *
 *  @param[in]     thread_id    Index to one connection to the database.
 *  @param[in]     chunk_start  Midnight of the first day.
 *  @param[in]     chunk_end    Midnight of the day after the last one.
 *  @param[in,out] insert       The batch the availabilities are added to.
 */
void availability_thread::_build_availabilities_chunk(
    int thread_id,
    time_t chunk_start,
    time_t chunk_end,
    database::bulk_or_multi& insert) {
  _logger->debug(
      "BAM-BI: availability thread writing availabilities for days {}-{}",
      chunk_start, chunk_end);

  /* A duration, or an open event laid out on one of its BA's periods (end is
   * then 0: the builder reads it as "until the end of the day"). */
  struct row {
    uint32_t ba_id;
    uint32_t timeperiod_id;
    time_t start;
    time_t end;
    short status;
    bool in_downtime;
    bool timeperiod_is_default;
    timeperiod_ptr tp;
  };
  std::vector<row> durations;
  std::vector<row> open_events;

  // The durations of finished events overlapping the range.
  std::string query(fmt::format(
      "SELECT b.ba_id, a.start_time, a.end_time, a.timeperiod_id, "
      "a.timeperiod_is_default, b.status, b.in_downtime "
      "FROM mod_bam_reporting_ba_events_durations AS a INNER JOIN "
      "mod_bam_reporting_ba_events AS b ON a.ba_event_id=b.ba_event_id "
      "AND b.end_time IS NOT NULL WHERE a.start_time<{} AND a.end_time>={} "
      "{} ORDER BY a.start_time",
      chunk_end, chunk_start,
      _should_rebuild_all ? fmt::format("AND b.ba_id IN({})", _bas_to_rebuild)
                          : ""));
  _logger->debug("Query: {}", query);
  {
    std::promise<database::mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    _mysql->run_query_and_get_result(query, std::move(promise), thread_id);
    try {
      database::mysql_result res(future.get());
      while (_mysql->fetch_row(res)) {
        uint32_t timeperiod_id = res.value_as_i32(3);
        timeperiod_ptr tp = _shared_tps.get_timeperiod(timeperiod_id);
        // No timeperiod found, skip.
        if (!tp) {
          _logger->debug("no timeperiod found with id {}", timeperiod_id);
          continue;
        }
        durations.push_back(
            row{static_cast<uint32_t>(res.value_as_i32(0)), timeperiod_id,
                res.value_as_i32(1), res.value_as_i32(2),
                static_cast<short>(res.value_as_i32(5)), res.value_as_bool(6),
                res.value_as_bool(4), std::move(tp)});
      }
    } catch (const std::exception& e) {
      throw msg_fmt("BAM-BI: availability thread could not build the data {}",
                    e.what());
    }
  }

  // The events not finished, on every period of their BA.
  query = fmt::format(
      "SELECT ba_id, start_time, status, in_downtime "
      "FROM mod_bam_reporting_ba_events WHERE start_time<{} AND "
      "end_time IS NULL {}",
      chunk_end,
      _should_rebuild_all ? fmt::format("AND ba_id IN ({})", _bas_to_rebuild)
                          : "");
  _logger->debug("Query: {}", query);
  {
    std::promise<database::mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    _mysql->run_query_and_get_result(query, std::move(promise), thread_id);
    try {
      database::mysql_result res(future.get());
      while (_mysql->fetch_row(res)) {
        uint32_t ba_id = res.value_as_i32(0);
        for (const ba_timeperiod& tp :
             _shared_tps.get_timeperiods_by_ba_id(ba_id))
          open_events.push_back(row{ba_id, tp.id, res.value_as_i32(1), 0,
                                    static_cast<short>(res.value_as_i32(2)),
                                    res.value_as_bool(3), tp.is_default,
                                    tp.tp});
      }
    } catch (const std::exception& e) {
      throw msg_fmt("BAM-BI: availability thread could not build the data: {}",
                    e.what());
    }
  }
  _logger->debug("BAM-BI: {} durations and {} open events for days {}-{}",
                 durations.size(), open_events.size(), chunk_start, chunk_end);

  /* Walk the days. The durations are sorted by start: those starting before
   * the end of the day join the active set as the day advances, and leave it
   * once their end is behind the day. */
  std::vector<const row*> active;
  size_t next = 0;
  for (time_t day_start = chunk_start; day_start < chunk_end;) {
    time_t day_end = add_round_days_to_midnight(day_start, 1);
    while (next < durations.size() && durations[next].start < day_end)
      active.push_back(&durations[next++]);
    active.erase(std::remove_if(
                     active.begin(), active.end(),
                     [day_start](const row* r) { return r->end < day_start; }),
                 active.end());

    // One builder per (BA, period) for this day.
    absl::btree_map<std::pair<uint32_t, uint32_t>,
                    std::unique_ptr<availability_builder>>
        builders;
    auto builder_of = [&](const row& r) -> availability_builder& {
      std::unique_ptr<availability_builder>& b =
          builders[{r.ba_id, r.timeperiod_id}];
      if (!b)
        b = std::make_unique<availability_builder>(day_end, day_start);
      return *b;
    };
    for (const row* r : active) {
      availability_builder& b = builder_of(*r);
      b.add_event(r->status, r->start, r->end, r->in_downtime, r->tp, _logger);
      b.set_timeperiod_is_default(r->timeperiod_is_default);
    }
    for (const row& r : open_events) {
      if (r.start >= day_end)
        continue;
      availability_builder& b = builder_of(r);
      b.add_event(r.status, r.start, 0, r.in_downtime, r.tp, _logger);
      b.set_timeperiod_is_default(r.timeperiod_is_default);
    }

    for (const auto& [key, builder] : builders)
      _write_availability(insert, *builder, key.first, day_start, key.second);
    day_start = day_end;
  }
  if (insert.row_count())
    insert.execute(*_mysql, database::mysql_error::insert_availability,
                   thread_id);
}

/**
 *  Add an availability to the batch. *One* row by ba, period and day.
 *
 *  @param[in,out] insert         The batch.
 *  @param[in]     builder        The builder of an availability.
 *  @param[in]     ba_id          The id of the ba.
 *  @param[in]     day_start      The start of the day.
 *  @param[in]     timeperiod_id  The id of the timeperiod.
 */
void availability_thread::_write_availability(
    database::bulk_or_multi& insert,
    availability_builder const& builder,
    uint32_t ba_id,
    time_t day_start,
    uint32_t timeperiod_id) {
  _logger->trace(
      "BAM-BI: availability thread writing availability for BA {} at day {} "
      "(timeperiod {})",
      ba_id, day_start, timeperiod_id);
  insert.add_multi_row(fmt::format(
      "({},{},{},{},{},{},{},{},{},{},{},{},{})", ba_id, day_start,
      timeperiod_id, int(builder.get_timeperiod_is_default()),
      builder.get_available(), builder.get_unavailable(),
      builder.get_degraded(), builder.get_unknown(), builder.get_downtime(),
      builder.get_unavailable_opened(), builder.get_degraded_opened(),
      builder.get_unknown_opened(), builder.get_downtime_opened()));
}

/**
 *  Compute the next midnight.
 *
 *  @return  The next midnight.
 */
time_t availability_thread::_compute_next_midnight() {
  return add_round_days_to_midnight(misc::start_of_day(::time(nullptr)), 1);
}

/**
 *  Open the database.
 */
void availability_thread::_open_database() {
  // Add database connection.
  try {
    _mysql = std::make_unique<mysql>(_db_cfg);
  } catch (const std::exception& e) {
    throw msg_fmt(
        "BAM-BI: availability thread could not connect to "
        "reporting database '{}'",
        e.what());
  }
}

/**
 *  Close the database.
 */
void availability_thread::_close_database() {
  if (_mysql) {
    _mysql.reset();
  }
}
