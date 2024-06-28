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

#include "com/centreon/engine/commands/otel_interface.hh"
#include "data_point_fifo.hh"

namespace com::centreon::engine::modules::opentelemetry {

class data_point_fifo_container;

/**
 * @brief converter are asynchronous object created on each check
 * In order to not parse command line on each check, we parse it once and then
 * create a converter config that will be used to create converter
 *
 */
class check_result_builder_config
    : public commands::otel::check_result_builder_config {
 public:
  enum class converter_type {
    nagios_check_result_builder,
    centreon_agent_check_result_builder
  };

 private:
  const converter_type _type;

 public:
  check_result_builder_config(converter_type conv_type) : _type(conv_type) {}
  converter_type get_type() const { return _type; }
};

/**
 * @brief The goal of this converter is to convert otel metrics in result
 * This object is synchronous and asynchronous
 * if needed data are available, it returns a results otherwise it will call
 * callback passed in param
 * These objects are oneshot, their lifetime is the check duration
 *
 */
class otl_check_result_builder
    : public std::enable_shared_from_this<otl_check_result_builder> {
  const std::string _cmd_line;
  const uint64_t _command_id;
  const std::pair<std::string /*host*/, std::string /*service*/> _host_serv;
  const std::chrono::system_clock::time_point _timeout;
  const commands::otel::result_callback _callback;

 protected:
  std::shared_ptr<spdlog::logger> _logger;

  virtual bool _build_result_from_metrics(metric_name_to_fifo&,
                                          commands::result& res) = 0;

 public:
  otl_check_result_builder(const std::string& cmd_line,
                           uint64_t command_id,
                           const host& host,
                           const service* service,
                           std::chrono::system_clock::time_point timeout,
                           commands::otel::result_callback&& handler,
                           const std::shared_ptr<spdlog::logger>& logger);

  virtual ~otl_check_result_builder() = default;

  const std::string& get_cmd_line() const { return _cmd_line; }

  uint64_t get_command_id() const { return _command_id; }

  const std::string& get_host_name() const { return _host_serv.first; }
  const std::string& get_service_description() const {
    return _host_serv.second;
  }

  const std::pair<std::string, std::string>& get_host_serv() const {
    return _host_serv;
  }

  std::chrono::system_clock::time_point get_time_out() const {
    return _timeout;
  }

  bool sync_build_result_from_metrics(data_point_fifo_container& data_pts,
                                      commands::result& res);

  bool async_build_result_from_metrics(data_point_fifo_container& data_pts);
  void async_time_out();

  virtual void dump(std::string& output) const;

  static std::shared_ptr<otl_check_result_builder> create(
      const std::string& cmd_line,
      const std::shared_ptr<check_result_builder_config>& conf,
      uint64_t command_id,
      const host& host,
      const service* service,
      std::chrono::system_clock::time_point timeout,
      commands::otel::result_callback&& handler,
      const std::shared_ptr<spdlog::logger>& logger);

  static std::shared_ptr<check_result_builder_config>
  create_check_result_builder_config(const std::string& cmd_line);
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
