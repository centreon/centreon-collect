/**
 * Copyright 2011-2013,2017-2024 Centreon
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
#ifndef CCE_CONFIGURATION_CONTACTGROUP_HH
#define CCE_CONFIGURATION_CONTACTGROUP_HH

#include "group.hh"
#include "object.hh"

namespace com::centreon::engine::configuration {

class contactgroup : public object {
 public:
  typedef std::string key_type;

  contactgroup(key_type const& key = "");
  contactgroup(contactgroup const& right);
  ~contactgroup() noexcept override;
  contactgroup& operator=(contactgroup const& right);
  bool operator==(contactgroup const& right) const noexcept;
  bool operator!=(contactgroup const& right) const noexcept;
  bool operator<(contactgroup const& right) const noexcept;
  void check_validity(error_cnt& err) const override;
  key_type const& key() const noexcept;
  void merge(object const& obj) override;
  bool parse(char const* key, char const* value) override;

  std::string const& alias() const noexcept;
  set_string& contactgroup_members() noexcept;
  set_string const& contactgroup_members() const noexcept;
  std::string const& contactgroup_name() const noexcept;
  set_string& members() noexcept;
  set_string const& members() const noexcept;

 private:
  typedef bool (*setter_func)(contactgroup&, char const*);

  bool _set_alias(std::string const& value);
  bool _set_contactgroup_members(std::string const& value);
  bool _set_contactgroup_name(std::string const& value);
  bool _set_members(std::string const& value);

  std::string _alias;
  group<set_string> _contactgroup_members;
  std::string _contactgroup_name;
  group<set_string> _members;
  static std::unordered_map<std::string, setter_func> const _setters;
};

typedef std::shared_ptr<contactgroup> contactgroup_ptr;
typedef std::set<contactgroup> set_contactgroup;

}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_CONTACTGROUP_HH
