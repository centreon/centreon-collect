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

#ifndef CCE_MOD_OTL_SERVER_OTL_FMT_HH
#define CCE_MOD_OTL_SERVER_OTL_FMT_HH

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
  template <typename FormatContext>
  auto format(const ::opentelemetry::proto::collector::metrics::v1::
                  ExportMetricsServiceRequest& p,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    return formatter<std::string>::format(
        max_length_log > 0 ? p.ShortDebugString().substr(0, max_length_log)
                           : p.ShortDebugString(),
        ctx);
  }
};

};  // namespace fmt

#endif
