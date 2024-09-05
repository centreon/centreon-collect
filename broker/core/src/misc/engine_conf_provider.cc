/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include "com/centreon/broker/misc/engine_conf_provider.hh"
#include <fmt/format.h>
#include <filesystem>
#include <string>
#include "common/engine_conf/parser.hh"

using namespace com::centreon::broker::misc;
using com::centreon::engine::configuration::parser;

engine_conf_provider::engine_conf_provider(
    const std::filesystem::path& pollers_conf_dir,
    const std::filesystem::path& local_pollers_conf_dir)
    : _pollers_conf_dir{pollers_conf_dir},
      _local_pollers_conf_dir{local_pollers_conf_dir} {}

/**
 * @brief Checks if the directory "local_pollers_conf_dir"/poller_id exists and
 * if its version is the one given. If the directory and the version match, we
 * know that Broker already handled this Engine version.
 *
 * @param poller_id The poller ID (needed to find the good directory).
 * @param version The Engine configuration version (currently a sha256 of the
 * directory).
 *
 * @return A boolean true on success, false otherwise.
 */
bool engine_conf_provider::knows_engine_conf(uint32_t poller_id,
                                             const std::string& version) const {
  std::filesystem::path poller_dir =
      _local_pollers_conf_dir / fmt::format("{}", poller_id);
  if (std::filesystem::exists(poller_dir) &&
      std::filesystem::is_directory(poller_dir)) {
    std::string local_version = parser::hash_directory(poller_dir);
    return local_version == version;
  }
  return false;
}

/**
 * @brief Checks if a new Engine configuration is available for the given poller
 * ID. If a new configuration is available, the working directory is updated
 * with it.
 *
 * @param poller_id The Engine poller ID.
 * @param version A pointer to a string, if not null filled with the new
 * version.
 *
 * @return
 */
bool engine_conf_provider::update_working_if_new_engine_conf(
    uint32_t poller_id,
    std::string* version) const {
  std::filesystem::path local_poller_dir =
      _local_pollers_conf_dir / fmt::format("{}", poller_id);
  std::string local_version;
  if (std::filesystem::exists(local_poller_dir) &&
      std::filesystem::is_directory(local_poller_dir))
    local_version = parser::hash_directory(local_poller_dir);

  std::filesystem::path poller_dir =
      _pollers_conf_dir / fmt::format("{}", poller_id);
  std::string new_version;
  if (std::filesystem::exists(poller_dir) &&
      std::filesystem::is_directory(poller_dir)) {
    std::filesystem::path working = working_dir();
    std::filesystem::copy(poller_dir, working,
                          std::filesystem::copy_options::recursive);

    new_version = parser::hash_directory(working);
  }
  if (new_version.empty())
    return false;
  if (version)
    *version = new_version;
  return new_version != local_version;
}
