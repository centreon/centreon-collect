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
#ifndef CCE_CONFIGURATION_COMMAND_HH
#define CCE_CONFIGURATION_COMMAND_HH

#include "object.hh"

namespace com::centreon::engine {

namespace configuration {
class command : public object {
 public:
  typedef std::string key_type;

  command(key_type const& key = "");
  command(command const& right);
  ~command() throw() override;
  command& operator=(command const& right);
  bool operator==(command const& right) const throw();
  bool operator!=(command const& right) const throw();
  bool operator<(command const& right) const throw();
  void check_validity(error_cnt& err) const override;
  key_type const& key() const throw();
  void merge(object const& obj) override;
  bool parse(char const* key, char const* value) override;

  std::string const& command_line() const throw();
  std::string const& command_name() const throw();
  std::string const& connector() const throw();

 private:
  typedef bool (*setter_func)(command&, char const*);

  bool _set_command_line(std::string const& value);
  bool _set_command_name(std::string const& value);
  bool _set_connector(std::string const& value);

  std::string _command_line;
  std::string _command_name;
  std::string _connector;
  static std::unordered_map<std::string, setter_func> const _setters;
};

using command_ptr = std::shared_ptr<command>;
using set_command = std::set<command>;
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_COMMAND_HH
