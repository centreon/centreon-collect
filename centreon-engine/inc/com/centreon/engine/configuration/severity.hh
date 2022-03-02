/*
** Copyright 2021 Centreon
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

#ifndef CCE_CONFIGURATION_SEVERITY_HH
#define CCE_CONFIGURATION_SEVERITY_HH

#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <string>
#include "com/centreon/engine/configuration/object.hh"
#include "com/centreon/engine/namespace.hh"

CCE_BEGIN()

namespace configuration {
class severity : public object {
  typedef bool (*setter_func)(severity&, const char*);
  using key_type = int32_t;
  key_type _id;
  int32_t _level;
  std::string _name;

  bool _set_id(int32_t id);
  bool _set_level(int32_t level);
  bool _set_name(const std::string& name);

 public:
  severity(const key_type& id = 0);
  severity(const severity& other);
  ~severity() noexcept override = default;
  severity& operator=(const severity& other);
  bool operator==(const severity& other) const noexcept;
  bool operator!=(const severity& other) const noexcept;
  bool operator<(const severity& other) const noexcept;
  void check_validity() const override;
  const key_type& key() const noexcept;
  void merge(const object& obj) override;
  bool parse(const char* key, const char* value) override;

  int32_t level() const noexcept;
  const std::string& name() const noexcept;

  static const absl::flat_hash_map<std::string, setter_func> _setters;
};

typedef absl::btree_set<severity> set_severity;
}  // namespace configuration

CCE_END()

#endif  // !CCE_CONFIGURATION_SEVERITY_HH
