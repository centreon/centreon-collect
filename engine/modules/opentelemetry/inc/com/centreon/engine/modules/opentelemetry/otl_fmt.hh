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
struct formatter<
    ::opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest>
    : formatter<std::string> {
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
    if (json_grpc_format) {
      std::string output;
      (void)google::protobuf::util::MessageToJsonString(p, &output);
      return formatter<std::string>::format(
          max_length_log > 0 ? output.substr(0, max_length_log) : output, ctx);
    } else {
      return formatter<std::string>::format(
          max_length_log > 0 ? p.ShortDebugString().substr(0, max_length_log)
                             : p.ShortDebugString(),
          ctx);
    }
  }
};

template <>
struct formatter<com::centreon::agent::MessageFromAgent>
    : formatter<std::string> {
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

    if (otl_formatter::json_grpc_format) {
      std::string output;
      (void)google::protobuf::util::MessageToJsonString(p, &output);
      return formatter<std::string>::format(
          otl_formatter::max_length_log > 0
              ? output.substr(0, otl_formatter::max_length_log)
              : output,
          ctx);
    } else {
      return formatter<std::string>::format(
          otl_formatter::max_length_log > 0
              ? p.ShortDebugString().substr(0, otl_formatter::max_length_log)
              : p.ShortDebugString(),
          ctx);
    }
  }
};

template <>
struct formatter<com::centreon::agent::MessageToAgent>
    : formatter<std::string> {
  /**
   * @brief if this static parameter is < 0, we dump all request, otherwise, we
   * limit dump length to this value
   *
   */
  template <typename FormatContext>
  auto format(const com::centreon::agent::MessageToAgent& p,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    using otl_formatter =
        formatter< ::opentelemetry::proto::collector::metrics::v1::
                       ExportMetricsServiceRequest>;

    if (otl_formatter::json_grpc_format) {
      std::string output;
      (void)google::protobuf::util::MessageToJsonString(p, &output);
      return formatter<std::string>::format(
          otl_formatter::max_length_log > 0
              ? output.substr(0, otl_formatter::max_length_log)
              : output,
          ctx);
    } else {
      return formatter<std::string>::format(
          otl_formatter::max_length_log > 0
              ? p.ShortDebugString().substr(0, otl_formatter::max_length_log)
              : p.ShortDebugString(),
          ctx);
    }
  }
};

};  // namespace fmt

#endif
