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

#ifndef CCE_MOD_OTL_SERVER_OTL_FMT_HH
#define CCE_MOD_OTL_SERVER_OTL_FMT_HH

#include <google/protobuf/util/json_util.h>
#include "com/centreon/common/fmt_protobuf.hh"

namespace com::centreon::engine::modules::opentelemetry {
struct otl_formatter {
  const ::google::protobuf::Message& mess;

  /**
   * @brief if this static parameter is < 0, we dump all request, otherwise, we
   * limit dump length to this value
   *
   */
  static int max_length_log;
  static bool json_grpc_format;
};

}  // namespace com::centreon::engine::modules::opentelemetry

namespace fmt {

/**
 * @brief this specialization is used by fmt to dump an
 * ExportMetricsServiceRequest
 *
 * @code {.c++}
 *      ::opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest
 * request; SPDLOG_LOGGER_TRACE(log_v2::otl(), "receive {}",
 * otl_formatter{request});
 * @endcode
 *
 *
 */
template <>
struct formatter<com::centreon::engine::modules::opentelemetry::otl_formatter> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }
  template <typename FormatContext>
  auto format(
      const com::centreon::engine::modules::opentelemetry::otl_formatter& mess,
      FormatContext& ctx) const -> decltype(ctx.out()) {
    auto out = ctx.out();
    com::centreon::common::back_iterator_output_stream<
        fmt::format_context::iterator>
        output_stream(out, mess.max_length_log);
    if (mess.json_grpc_format) {
      [[maybe_unused]] auto ignored =
          ::google::protobuf::json::MessageToJsonStream(mess.mess,
                                                        &output_stream);
    } else {
      google::protobuf::TextFormat::Printer printer;
      printer.SetSingleLineMode(true);
      printer.Print(mess.mess, &output_stream);
    }
    return out;
  }
};

};  // namespace fmt

#endif
