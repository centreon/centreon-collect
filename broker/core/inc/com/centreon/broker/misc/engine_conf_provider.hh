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
#ifndef CCB_MISC_ENGINE_CONF_PROVIDER_HH
#define CCB_MISC_ENGINE_CONF_PROVIDER_HH

#include <filesystem>
namespace com::centreon::broker::misc {
class engine_conf_provider {
  std::filesystem::path _pollers_conf_dir;
  std::filesystem::path _local_pollers_conf_dir;

 public:
  engine_conf_provider(const std::filesystem::path& pollers_conf_dir,
                       const std::filesystem::path& local_pollers_conf_dir);
  const std::filesystem::path& local_pollers_conf_dir() const {
    return _local_pollers_conf_dir;
  }
  const std::filesystem::path& pollers_conf_dir() const {
    return _pollers_conf_dir;
  }
  std::filesystem::path working_dir() const {
    return _local_pollers_conf_dir / "working";
  }
  bool knows_engine_conf(uint32_t poller_id, const std::string& version) const;
  bool update_working_if_new_engine_conf(uint32_t poller_id, std::string* version) const;
};
}  // namespace com::centreon::broker::misc

#endif /* !CCB_MISC_ENGINE_CONF_PROVIDER_HH */
