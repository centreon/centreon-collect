/*
 * Copyright 2021 Centreon (https://www.centreon.com/)
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
#include <memory>
#include <ostream>
#include "com/centreon/engine/namespace.hh"

/* Forward declaration. */
namespace com::centreon::engine {
class severity;
}

using severity_map =
    absl::flat_hash_map<uint32_t,
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
  uint32_t _id;
  uint32_t _level;
  std::string _name;

 public:
  static severity_map severities;

  severity(int32_t id, int32_t level, const std::string& name);
  ~severity() noexcept = default;
  severity(const severity&) = delete;
  severity& operator=(const severity&) = delete;
  bool operator==(const severity&) = delete;
  bool operator!=(const severity&) = delete;

  uint32_t id() const;
  const std::string& name() const;
  void set_name(const std::string& name);
  uint32_t level() const;
  void set_level(uint32_t level);
};

CCE_END()

std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::severity const& obj);
std::ostream& operator<<(std::ostream& os, severity_map_unsafe const& obj);

#endif  // !CCE_SEVERITY_HH
