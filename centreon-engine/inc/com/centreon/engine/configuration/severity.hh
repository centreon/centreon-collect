/*
** Copyright 2021 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
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
