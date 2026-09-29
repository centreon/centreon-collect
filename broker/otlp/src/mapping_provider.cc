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

#include "com/centreon/broker/otlp/mapping_provider.hh"

using namespace com::centreon::broker::otlp;

/**
 * @brief Construct a provider serving table. It does not watch path: load()
 * starts the watcher.
 *
 * @param path mapping file, empty when there is none
 * @param table initial table
 * @param logger
 */
mapping_provider::mapping_provider(
    const std::filesystem::path& path,
    const mapping_table::pointer& table,
    const std::shared_ptr<spdlog::logger>& logger)
    : _path(path), _logger(logger), _table(table) {}

/**
 * @brief Stop the file watcher, if any.
 */
mapping_provider::~mapping_provider() {
  if (_watcher)
    _watcher->stop();
}

/**
 * @brief Provider serving the empty table, with no file to watch: every metric
 * falls back to centreon.*.
 *
 * @param logger
 * @return the provider
 */
mapping_provider::pointer mapping_provider::empty(
    const std::shared_ptr<spdlog::logger>& logger) {
  return std::make_shared<mapping_provider>(std::filesystem::path(),
                                            mapping_table::empty(), logger);
}

/**
 * @brief Read the mapping file and start watching it.
 *
 * @param io_context context running the file watcher and the reloads
 * @param path mapping file
 * @param logger
 * @return the provider
 * @throw msg_fmt if the file can't be read or is not a valid mapping
 */
mapping_provider::pointer mapping_provider::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::filesystem::path& path,
    const std::shared_ptr<spdlog::logger>& logger) {
  mapping_table::pointer table = mapping_table::from_file(path);
  SPDLOG_LOGGER_INFO(logger, "otlp: {} metric mappings loaded from {}",
                     table->size(), path.string());
  auto provider = std::make_shared<mapping_provider>(path, table, logger);
  provider->_start_watcher(io_context);
  return provider;
}

/**
 * @brief Watch the mapping file and reload it on each change.
 *
 * The handler holds a weak pointer, so it does nothing once the provider is
 * destroyed.
 *
 * @param io_context context running the watcher
 */
void mapping_provider::_start_watcher(
    const std::shared_ptr<asio::io_context>& io_context) {
  _watcher = com::centreon::common::file_watcher::load(
      io_context, _logger, _path,
      [me = std::weak_ptr<mapping_provider>(shared_from_this())]() {
        if (auto provider = me.lock())
          provider->reload();
      });
}

/**
 * @brief Current mapping table.
 *
 * @return the table; it stays valid after a reload replaces it
 */
mapping_table::pointer mapping_provider::get() const {
  absl::MutexLock lock(_protect);
  return _table;
}

/**
 * @brief Re-read the mapping file and replace the table if the file is valid.
 * Otherwise the error is logged and the current table is kept.
 *
 * @return true if the table was replaced
 */
bool mapping_provider::reload() {
  if (_path.empty())
    return false;
  mapping_table::pointer table;
  try {
    table = mapping_table::from_file(_path);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "otlp: {}; keeping the previous mapping",
                        e.what());
    return false;
  }
  SPDLOG_LOGGER_INFO(_logger, "otlp: {} metric mappings reloaded from {}",
                     table->size(), _path.string());
  {
    absl::MutexLock lock(_protect);
    /* swap so that the old table is released outside the lock */
    std::swap(_table, table);
  }
  /* the old table can still be used by request_builder to map the semconv
    it's will be destroyed after the last metric
  */
  return true;
}
