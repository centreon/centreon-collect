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
#ifndef CCE_CONFIGURATION_HOSTGROUP_HH
#define CCE_CONFIGURATION_HOSTGROUP_HH

#include "com/centreon/common/opt.hh"
#include "group.hh"
#include "object.hh"

namespace com::centreon::engine {

namespace configuration {
class hostgroup : public object {
 public:
  typedef std::string key_type;

  hostgroup(key_type const& key = "");
  hostgroup(hostgroup const& right);
  ~hostgroup() throw() override;
  hostgroup& operator=(hostgroup const& right);
  bool operator==(hostgroup const& right) const throw();
  bool operator!=(hostgroup const& right) const throw();
  bool operator<(hostgroup const& right) const throw();
  void check_validity(error_cnt& err) const override;
  key_type const& key() const throw();
  void merge(object const& obj) override;
  bool parse(char const* key, char const* value) override;

  std::string const& action_url() const throw();
  std::string const& alias() const throw();
  unsigned int hostgroup_id() const throw();
  std::string const& hostgroup_name() const throw();
  set_string& members() throw();
  set_string const& members() const throw();
  std::string const& notes() const throw();
  std::string const& notes_url() const throw();

 private:
  typedef bool (*setter_func)(hostgroup&, char const*);

  bool _set_action_url(std::string const& value);
  bool _set_alias(std::string const& value);
  bool _set_hostgroup_id(unsigned int value);
  bool _set_hostgroup_name(std::string const& value);
  bool _set_members(std::string const& value);
  bool _set_notes(std::string const& value);
  bool _set_notes_url(std::string const& value);

  std::string _action_url;
  std::string _alias;
  unsigned int _hostgroup_id;
  std::string _hostgroup_name;
  group<set_string> _members;
  std::string _notes;
  std::string _notes_url;
  static std::unordered_map<std::string, setter_func> const _setters;
};

typedef std::shared_ptr<hostgroup> hostgroup_ptr;
typedef std::set<hostgroup> set_hostgroup;
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_HOSTGROUP_HH
