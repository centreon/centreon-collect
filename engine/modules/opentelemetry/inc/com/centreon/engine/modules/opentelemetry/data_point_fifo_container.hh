/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 * You may obtain a copy of the
 License at

 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */
#ifndef CCE_MOD_OTL_SERVER_DATA_POINT_FIFO_CONTAINER_HH
#define CCE_MOD_OTL_SERVER_DATA_POINT_FIFO_CONTAINER_HH

#include "data_point_fifo.hh"

namespace com::centreon::engine::modules::opentelemetry {

/**
 * @brief This class is a
 * map host_serv -> map metric -> data_point_fifo (list of data_points)
 *
 */
class data_point_fifo_container {
 public:
 private:
  /**
   * @brief
   * metrics are ordered like this:
   * <host, serv> => metric1 => data_points list
   *              => metric2 => data_points list
   *
   */
  using host_serv_to_metrics = absl::flat_hash_map<host_serv,
                                                   metric_name_to_fifo,
                                                   host_serv_hash_eq,
                                                   host_serv_hash_eq>;

  host_serv_to_metrics _data;

  static metric_name_to_fifo _empty;

  std::mutex _data_m;

 public:
  void clean();

  static void clean_empty_fifos(metric_name_to_fifo& to_clean);

  void add_data_point(const std::string_view& host,
                      const std::string_view& service,
                      const std::string_view& metric,
                      const otl_data_point& data_pt);

  const metric_name_to_fifo& get_fifos(const std::string& host,
                                       const std::string& service) const;

  metric_name_to_fifo& get_fifos(const std::string& host,
                                 const std::string& service);

  void lock() { _data_m.lock(); }

  void unlock() { _data_m.unlock(); }

  void dump(std::string& output) const;
};

}  // namespace com::centreon::engine::modules::opentelemetry

namespace fmt {
template <>
struct formatter<
    com::centreon::engine::modules::opentelemetry::data_point_fifo_container>
    : formatter<std::string> {
  template <typename FormatContext>
  auto format(const com::centreon::engine::modules::opentelemetry::
                  data_point_fifo_container& cont,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    std::string output;
    cont.dump(output);
    return formatter<std::string>::format(output, ctx);
  }
};

}  // namespace fmt

#endif
