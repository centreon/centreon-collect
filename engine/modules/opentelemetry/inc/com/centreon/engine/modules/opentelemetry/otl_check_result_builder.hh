/*
** Copyright 2024 Centreon
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

#ifndef CCE_MOD_OTL_CHECK_RESULT_BUILDER_HH
#define CCE_MOD_OTL_CHECK_RESULT_BUILDER_HH

#include "com/centreon/engine/check_result.hh"

#include "com/centreon/engine/commands/otel_interface.hh"
#include "otl_data_point.hh"

namespace com::centreon::engine::modules::opentelemetry {

/**
 * @brief compare data_points with nano_timestamp
 *
 */
struct otl_data_point_pointer_compare {
  using is_transparent = void;

  bool operator()(const otl_data_point& left,
                  const otl_data_point& right) const {
    return left.get_nano_timestamp() < right.get_nano_timestamp();
  }

  bool operator()(const otl_data_point& left, uint64_t right) const {
    return left.get_nano_timestamp() < right;
  }

  bool operator()(uint64_t left, const otl_data_point& right) const {
    return left < right.get_nano_timestamp();
  }
};

class metric_to_datapoints
    : public absl::flat_hash_map<
          std::string_view,
          absl::btree_multiset<otl_data_point,
                               otl_data_point_pointer_compare>> {};

/**
 * @brief The goal of this converter is to convert otel metrics in result
 * This object is synchronous and asynchronous
 * if needed data are available, it returns a results otherwise it will call
 * callback passed in param
 * These objects are oneshot, their lifetime is the check duration
 *
 */
class otl_check_result_builder
    : public commands::otel::otl_check_result_builder_base {
  const std::string _cmd_line;

 protected:
  std::shared_ptr<spdlog::logger> _logger;

 public:
  otl_check_result_builder(const std::string& cmd_line,
                           const std::shared_ptr<spdlog::logger>& logger);

  virtual ~otl_check_result_builder() = default;

  const std::string& get_cmd_line() const { return _cmd_line; }

  virtual void dump(std::string& output) const;

  void process_data_pts(const std::string_view& host,
                        const std::string_view& serv,
                        const metric_to_datapoints& data_pts) override;

  static std::shared_ptr<otl_check_result_builder> create(
      const std::string& cmd_line,
      const std::shared_ptr<spdlog::logger>& logger);

  virtual bool build_result_from_metrics(const metric_to_datapoints& data_pts,
                                         check_result& res) = 0;
};

}  // namespace com::centreon::engine::modules::opentelemetry

namespace fmt {

template <>
struct formatter<
    com::centreon::engine::modules::opentelemetry::otl_check_result_builder>
    : formatter<std::string> {
  template <typename FormatContext>
  auto format(const com::centreon::engine::modules::opentelemetry::
                  otl_check_result_builder& cont,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    std::string output;
    (&cont)->dump(output);
    return formatter<std::string>::format(output, ctx);
  }
};

}  // namespace fmt

#endif
