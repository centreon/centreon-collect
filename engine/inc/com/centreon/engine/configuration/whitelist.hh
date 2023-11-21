/*
 * Copyright 2023 Centreon (https://www.centreon.com/)
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
#ifndef CCE_CONFIGURATION_WHITELIST_HH
#define CCE_CONFIGURATION_WHITELIST_HH

#include "com/centreon/engine/namespace.hh"

CCE_BEGIN()

namespace configuration {

extern const std::string command_blacklist_output;

/**
 * @brief the goal of this class is to parse yaml or json file with the
structure whitelist: wildcard:
    - /usr/lib/centreon/plugins/centreon_toto*titi
    - /usr/lib/centreon/plugins/centreon_toto*tata*
  regex:
    - /usr/lib/centreon/plugins/centreon_\d{5}.*
    -  /usr/lib/centreon/plugins/check_centreon_bam

When this kind of file is present in /etc/centreon-engine-whitelist directory
commands are executed only if they match to at least one wildcard or regex
string
 *
 */
class whitelist_file {
  std::string _path;
  time_t _last_file_write;

  std::vector<std::string> _wildcards;
  std::vector<std::unique_ptr<re2::RE2>> _regex;

  template <class ryml_tree>
  void _read_file_content(const ryml_tree& file_content);

  static void init_ryml_error_handler();

 public:
  struct open_file_exception : public virtual boost::exception,
                               public virtual std::exception {};

  struct yaml_structure_exception : public virtual boost::exception,
                                    public virtual std::exception {};

  template <typename str>
  whitelist_file(const str& path) : _path(path) {}

  void parse();

  bool test(const std::string& cmdline) const;

  template <typename str>
  static std::unique_ptr<whitelist_file> create(const str& path);

  time_t get_last_file_write() const { return _last_file_write; }

  const std::vector<std::string> get_wildcards() const { return _wildcards; }
  const std::string& get_path() const { return _path; }
  bool empty() const { return _wildcards.empty() && _regex.empty(); }
};

/**
 * @brief contains one whitelist_file instance by file found in _path directory
 * beware: search isn't recursive
 *
 */
class whitelist_directory {
  std::string _path;

  std::vector<std::unique_ptr<whitelist_file>> _files;

 public:
  struct bad_directory_exception : public virtual boost::exception,
                                   public virtual std::exception {};

  whitelist_directory(const std::string& path) : _path(path) {}

  // don't reorder values
  enum e_refresh_result { no_directory, empty_directory, no_rule, rules };

  e_refresh_result refresh();

  const std::string& get_path() const { return _path; }

  bool test(const std::string& cmdline) const;

  bool empty() const { return _files.empty(); }

  const std::vector<std::unique_ptr<whitelist_file>>& get_files() const {
    return _files;
  }
};

/**
 * @brief scan several whitelist directories
 *
 */
class whitelist_directories {
  std::list<whitelist_directory> _directories;

 public:
  template <class dir_const_iter>
  whitelist_directories(dir_const_iter begin, dir_const_iter end);

  bool test(const std::string& cmdline) const;

  void refresh();

  const std::list<whitelist_directory>& get_directories() const {
    return _directories;
  }
};

template <class dir_const_iter>
whitelist_directories::whitelist_directories(dir_const_iter begin,
                                             dir_const_iter end) {
  for (; begin != end; ++begin) {
    _directories.emplace_back(*begin);
  }
}

};  // namespace configuration

CCE_END()

#endif
