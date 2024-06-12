/**
 * Copyright 2024 Centreon
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

#include "data_point_fifo.hh"

using namespace com::centreon::engine::modules::opentelemetry;

time_t data_point_fifo::_second_datapoint_expiry = 600;
size_t data_point_fifo::_max_size = 2;

/**
 * @brief opentelemetry fifo limits share a same value
 * The goal of this isto fix these limits
 *
 * @param second_datapoint_expiry
 * @param max_size
 */
void data_point_fifo::update_fifo_limit(time_t second_datapoint_expiry,
                                        size_t max_size) {
  _second_datapoint_expiry = second_datapoint_expiry;
  _max_size = max_size;
}

/**
 * @brief add one data point to fifo
 *
 * @param data_pt
 */
void data_point_fifo::add_data_point(const otl_data_point& data_pt) {
  clean();
  _fifo.insert(data_pt);
}

/**
 * @brief erase to older data points
 *
 */
void data_point_fifo::clean() {
  if (!_fifo.empty()) {
    auto first = _fifo.begin();
    time_t expiry = time(nullptr) - _second_datapoint_expiry;
    if (expiry < 0) {
      expiry = 0;
    }

    while (!_fifo.empty() &&
           first->get_nano_timestamp() / 1000000000 < expiry) {
      first = _fifo.erase(first);
    }

    if (_fifo.size() >= _max_size) {
      _fifo.erase(first);
    }
  }
}

/**
 * @brief erase oldest element
 *
 * @param expiry  data points older than this nano timestamp are erased
 */
void data_point_fifo::clean_oldest(uint64_t expiry) {
  while (!_fifo.empty() && _fifo.begin()->get_nano_timestamp() < expiry) {
    _fifo.erase(_fifo.begin());
  }
}
