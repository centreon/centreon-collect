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

#include "data_point_fifo_container.hh"

using namespace com::centreon::engine::modules::otl_server;

metric_name_to_fifo data_point_container::_empty;

/**
 * @brief clean olds data_points
 * no need to lock mutex
 */
void data_point_container::clean() {
  std::lock_guard l(_data_m);
  for (auto& serv_to_fifos : _data) {
    for (auto& fifo : serv_to_fifos.second) {
      fifo.second.clean();
    }
  }
}

/**
 * @brief add a data point in the corresponding fifo
 * mutex must be locked during returned data use
 *
 * @param data_pt
 */
void data_point_container::add_data_point(const std::string_view& host,
                                          const std::string_view& service,
                                          const std::string_view& metric,
                                          const data_point& data_pt) {
  metric_name_to_fifo& fifos = _data[std::make_pair(host, service)];
  auto exist = fifos.find(metric);
  if (exist == fifos.end()) {
    exist = fifos.emplace(metric, data_point_fifo()).first;
  }
  exist->second.add_data_point(data_pt);
}

/**
 * @brief get all fifos of a service
 * mutex must be locked during returned data use
 *
 * @param host
 * @param service
 * @return const metric_name_to_fifo&
 */
const metric_name_to_fifo& data_point_container::get_fifos(
    const std::string& host,
    const std::string& service) const {
  auto exist = _data.find({host, service});
  return exist == _data.end() ? _empty : exist->second;
}

/**
 * @brief get all fifos of a service
 * mutex must be locked during returned data use
 *
 * @param host
 * @param service
 * @return metric_name_to_fifo&
 */
metric_name_to_fifo& data_point_container::get_fifos(
    const std::string& host,
    const std::string& service) {
  auto exist = _data.find({host, service});
  return exist == _data.end() ? _empty : exist->second;
}
