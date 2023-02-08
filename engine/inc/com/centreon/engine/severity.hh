/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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

#ifndef CCE_SEVERITY_HH
#define CCE_SEVERITY_HH

#include <absl/container/flat_hash_map.h>

/* Forward declaration. */
namespace com {
namespace centreon {
namespace engine {
class severity;
}
}  // namespace centreon
}  // namespace com

using severity_map =
    absl::flat_hash_map<std::pair<uint32_t, uint16_t>,
                        std::shared_ptr<com::centreon::engine::severity>>;
using severity_map_unsafe =
    absl::flat_hash_map<std::string, com::centreon::engine::severity*>;

CCE_BEGIN()

/**
 *  @class severity severity.hh "com/centreon/engine/severity.hh
 *  @brief Object representing a severity
 *
 */
class severity {
 public:
  enum severity_type { none = -1, service = 0, host = 1 };

  uint64_t _id;
  uint32_t _level;
  uint64_t _icon_id;
  std::string _name;
  severity_type _type;

 public:
  static severity_map severities;

  severity(uint64_t id,
           uint32_t level,
           uint64_t icon_id,
           const std::string& name,
           uint16_t type);
  ~severity() noexcept = default;
  severity(const severity&) = delete;
  severity& operator=(const severity&) = delete;
  bool operator==(const severity&) = delete;
  bool operator!=(const severity&) = delete;

  uint64_t id() const;
  const std::string& name() const;
  void set_name(const std::string& name);
  uint32_t level() const;
  uint64_t icon_id() const;
  void set_level(uint32_t level);
  void set_icon_id(uint64_t icon_id);
  void set_type(const severity_type typ);
  severity_type type() const;
};

CCE_END()

std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::severity const& obj);
std::ostream& operator<<(std::ostream& os, severity_map_unsafe const& obj);

#endif  // !CCE_SEVERITY_HH
