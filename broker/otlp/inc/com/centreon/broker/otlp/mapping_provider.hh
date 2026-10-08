/**
 * Copyright 2026 Centreon
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

#ifndef CCB_OTLP_MAPPING_PROVIDER_HH
#define CCB_OTLP_MAPPING_PROVIDER_HH

#include "com/centreon/broker/otlp/semconv_mapping.hh"
#include "com/centreon/common/file_watcher.hh"

namespace com::centreon::broker::otlp {

/**
 * @brief Gives the streams the mapping table.
 *
 * Without a mapping file it serves an empty table, so every metric is
 * exported under the centreon.* namespace. With one, the file is
 * read at creation and watched with common::file_watcher: each time it is
 * rewritten it is parsed again and, if valid, replaces the table in use. An
 * invalid rewrite is logged and the previous table is kept, so a typo in the
 * file never stops the export.
 *
 * get() may be called from any thread; reload happens on the io_context
 * thread.
 */
class mapping_provider : public std::enable_shared_from_this<mapping_provider> {
  const std::filesystem::path _path;
  std::shared_ptr<spdlog::logger> _logger;

  mutable absl::Mutex _protect;
  mapping_table::pointer _table ABSL_GUARDED_BY(_protect);

  std::shared_ptr<com::centreon::common::file_watcher> _watcher;

  void _start_watcher(const std::shared_ptr<asio::io_context>& io_context);

 public:
  using pointer = std::shared_ptr<mapping_provider>;

  mapping_provider(const std::filesystem::path& path,
                   const mapping_table::pointer& table,
                   const std::shared_ptr<spdlog::logger>& logger);
  ~mapping_provider();

  mapping_provider(const mapping_provider&) = delete;
  mapping_provider& operator=(const mapping_provider&) = delete;

  /**
   * @brief Provider serving the empty table, with nothing to watch.
   */
  static pointer empty(const std::shared_ptr<spdlog::logger>& logger);

  /**
   * @brief Read the mapping file and start watching it.
   *
   * @throw msg_fmt if the file can't be read or is not a valid mapping: a
   * broken configuration at startup is reported rather than silently replaced
   * by the empty table.
   */
  static pointer load(const std::shared_ptr<asio::io_context>& io_context,
                      const std::filesystem::path& path,
                      const std::shared_ptr<spdlog::logger>& logger);

  /**
   * @brief Table to map with. Take it once per batch of lookups: it stays
   * valid even if a reload replaces it meanwhile.
   */
  mapping_table::pointer get() const ABSL_LOCKS_EXCLUDED(_protect);

  /**
   * @brief Re-read the file. On failure the current table is kept.
   *
   * @return true if the table was replaced.
   */
  bool reload() ABSL_LOCKS_EXCLUDED(_protect);

  const std::filesystem::path& path() const { return _path; }
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_MAPPING_PROVIDER_HH
