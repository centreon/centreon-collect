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

#ifndef CCE_MOD_OTL_CONVERTER_HH
#define CCE_MOD_OTL_CONVERTER_HH

#include "com/centreon/engine/commands/otel_interface.hh"
#include "data_point_fifo.hh"

namespace com::centreon::engine::modules::otl_server {

class data_point_container;

/**
 * @brief The goal of this converter is to convert otel metrics in result
 * This object is synchronous and asynchronous
 * if needed data are available, it returns a results otherwise it will call
 * callback passed in param
 *
 */
class otl_converter : public std::enable_shared_from_this<otl_converter> {
  const std::string _cmd_line;
  const uint64_t _command_id;
  const std::pair<std::string /*host*/, std::string /*service*/> _host_serv;
  const std::chrono::system_clock::time_point _timeout;
  const commands::otel::result_callback _callback;

 protected:
  virtual bool _build_result_from_metrics(metric_name_to_fifo&,
                                          commands::result& res) = 0;

 public:
  otl_converter(const std::string& cmd_line,
                uint64_t command_id,
                const host& host,
                const service* service,
                std::chrono::system_clock::time_point timeout,
                commands::otel::result_callback&& handler);

  virtual ~otl_converter() = default;

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

  bool sync_build_result_from_metrics(data_point_container& data_pts,
                                      commands::result& res);

  bool async_build_result_from_metrics(data_point_container& data_pts);
  void async_time_out();

  static std::shared_ptr<otl_converter> create(
      const std::string& cmd_line,
      uint64_t command_id,
      const host& host,
      const service* service,
      std::chrono::system_clock::time_point timeout,
      commands::otel::result_callback&& handler);
};

class otl_nagios_telegraf_converter : public otl_converter {
 protected:
  bool _build_result_from_metrics(metric_name_to_fifo& fifos,
                                  commands::result& res) override;

 public:
  otl_nagios_telegraf_converter(const std::string& cmd_line,
                                uint64_t command_id,
                                const host& host,
                                const service* service,
                                std::chrono::system_clock::time_point timeout,
                                commands::otel::result_callback&& handler)
      : otl_converter(cmd_line,
                      command_id,
                      host,
                      service,
                      timeout,
                      std::move(handler)) {}
};

}  // namespace com::centreon::engine::modules::otl_server

namespace fmt {

template <>
struct formatter<com::centreon::engine::modules::otl_server::otl_converter> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  // Formats the point p using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(
      const com::centreon::engine::modules::otl_server::otl_converter& conv,
      FormatContext& ctx) const -> decltype(ctx.out()) {
    return format_to(ctx.out(), "command_id:{}, cmd_line: {}",
                     conv.get_command_id(), conv.get_cmd_line());
  }
};

}  // namespace fmt

#endif
