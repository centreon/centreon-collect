/*
** Copyright 2023 Centreon
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

#include "com/centreon/broker/database/stats.hh"

using namespace com::centreon::broker::database;

const std::vector<stats::stat_query>& stats::get_stat_query() const {
  return _stat_query;
}

float stats::average_query_duration() const {
  float retval = 0.0f;
  if (!_query_duration.empty()) {
    for (float d : _query_duration)
      retval += d;
    retval /= _query_duration.size();
  }
  return retval;
}

const std::vector<stats::stat_statement>& stats::get_stat_stmt() const {
  return _stat_stmt;
}

float stats::average_stmt_duration() const {
  float retval = 0.0f;
  if (!_stmt_duration.empty()) {
    for (float d : _stmt_duration)
      retval += d;
    retval /= _stmt_duration.size();
  }
  return retval;
}

/**
 * @brief Returns two averages:
 * * the average duration of a loop in seconds.
 * * the average activity in percents during a loop.
 * @return an average loop.
 */
stats::loop stats::average_loop() const {
  stats::loop retval{0, 0};

  if (!_loop.empty()) {
    for (auto& s : _loop) {
      retval.duration += s.duration;
      retval.activity_percent += s.activity_percent * s.duration;
    }
    /* We compute activity_percent at first because we need the whole
     * duration */
    retval.activity_percent /= retval.duration;
    /* Then we can compute the average duration */
    retval.duration /= _loop.size();
  }
  return retval;
}
