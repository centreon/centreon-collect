/**
 * Copyright 2011-2013,2017-2025 Centreon
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
#ifndef CCE_CONFIGURATION_SERVICEGROUP_HH
#define CCE_CONFIGURATION_SERVICEGROUP_HH

#include "com/centreon/common/opt.hh"
#include "group.hh"
#include "object.hh"

typedef std::set<std::pair<std::string, std::string> > set_pair_string;

namespace com::centreon::engine {

namespace configuration {
class servicegroup : public object {
 public:
  typedef std::string key_type;

  servicegroup(key_type const& key = "");
  servicegroup(servicegroup const& right);
  ~servicegroup() noexcept override;
  servicegroup& operator=(servicegroup const& right);
  bool operator==(servicegroup const& right) const throw();
  bool operator!=(servicegroup const& right) const throw();
  bool operator<(servicegroup const& right) const throw();
  void check_validity(error_cnt& err) const override;
  key_type const& key() const throw();
  void merge(object const& obj) override;
  bool parse(char const* key, char const* value) override;

  std::string const& action_url() const noexcept;
  std::string const& alias() const noexcept;
  set_pair_string& members() noexcept;
  set_pair_string const& members() const noexcept;
  std::string const& notes() const noexcept;
  std::string const& notes_url() const noexcept;
  unsigned int servicegroup_id() const noexcept;
  set_string& servicegroup_members() noexcept;
  set_string const& servicegroup_members() const noexcept;
  std::string const& servicegroup_name() const noexcept;

 private:
  typedef bool (*setter_func)(servicegroup&, char const*);

  bool _set_action_url(std::string const& value);
  bool _set_alias(std::string const& value);
  bool _set_members(std::string const& value);
  bool _set_notes(std::string const& value);
  bool _set_notes_url(std::string const& value);
  bool _set_servicegroup_id(unsigned int value);
  bool _set_servicegroup_members(std::string const& value);
  bool _set_servicegroup_name(std::string const& value);

  std::string _action_url;
  std::string _alias;
  group<set_pair_string> _members;
  std::string _notes;
  std::string _notes_url;
  unsigned int _servicegroup_id;
  group<set_string> _servicegroup_members;
  std::string _servicegroup_name;
  static std::unordered_map<std::string, setter_func> const _setters;
};

typedef std::shared_ptr<servicegroup> servicegroup_ptr;
typedef std::set<servicegroup> set_servicegroup;
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_SERVICEGROUP_HH
