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

namespace fmt {

/**
 * @brief this specialization is used by fmt to dump an
 * ExportMetricsServiceRequest
 *
 * @code {.c++}
 *      ::opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest
 * request; SPDLOG_LOGGER_TRACE(log_v2::otl(), "receive {}", request);
 * @endcode
 *
 *
 */
template <>
struct formatter< ::opentelemetry::proto::collector::metrics::v1::
                      ExportMetricsServiceRequest> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }
  /**
   * @brief if this static parameter is < 0, we dump all request, otherwise, we
   * limit dump length to this value
   *
   */
  static int max_length_log;
  static bool json_grpc_format;

  template <typename FormatContext>
  auto format(const ::opentelemetry::proto::collector::metrics::v1::
                  ExportMetricsServiceRequest& p,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    auto out = ctx.out();
    com::centreon::common::back_iterator_output_stream<
        fmt::format_context::iterator>
        output_stream(out, max_length_log);
    if (json_grpc_format) {
      [[maybe_unused]] auto ignored =
          ::google::protobuf::json::MessageToJsonStream(p, &output_stream);
    } else {
      google::protobuf::TextFormat::Printer printer;
      printer.SetSingleLineMode(true);
      printer.Print(p, &output_stream);
    }
    return out;
  }
};

template <>
struct formatter<com::centreon::agent::MessageFromAgent> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  /**
   * @brief if this static parameter is < 0, we dump all request, otherwise, we
   * limit dump length to this value
   *
   */
  template <typename FormatContext>
  auto format(const com::centreon::agent::MessageFromAgent& p,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    using otl_formatter =
        formatter< ::opentelemetry::proto::collector::metrics::v1::
                       ExportMetricsServiceRequest>;

    auto out = ctx.out();
    com::centreon::common::back_iterator_output_stream<
        fmt::format_context::iterator>
        output_stream(out, otl_formatter::max_length_log);
    if (otl_formatter::json_grpc_format) {
      [[maybe_unused]] auto ignored =
          ::google::protobuf::json::MessageToJsonStream(p, &output_stream);
    } else {
      google::protobuf::TextFormat::Printer printer;
      printer.SetSingleLineMode(true);
      printer.Print(p, &output_stream);
    }
    return out;
  }
};

template <>
struct formatter<com::centreon::agent::MessageToAgent> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }
  /**
   * @brief if this static parameter is < 0, we dump all request, otherwise,
   * we limit dump length to this value
   *
   */
  template <typename FormatContext>
  auto format(const com::centreon::agent::MessageToAgent& p,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    using otl_formatter =
        formatter< ::opentelemetry::proto::collector::metrics::v1::
                       ExportMetricsServiceRequest>;

    auto out = ctx.out();
    com::centreon::common::back_iterator_output_stream<
        fmt::format_context::iterator>
        output_stream(out, otl_formatter::max_length_log);
    if (otl_formatter::json_grpc_format) {
      [[maybe_unused]] auto ignored =
          ::google::protobuf::json::MessageToJsonStream(p, &output_stream);
    } else {
      google::protobuf::TextFormat::Printer printer;
      printer.SetSingleLineMode(true);
      printer.Print(p, &output_stream);
    }
    return out;
  }
};

};  // namespace fmt

#endif
