/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2024 Centreon
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
#ifndef CCE_CONFIGURATION_FILE_INFO_HH
#define CCE_CONFIGURATION_FILE_INFO_HH

namespace com::centreon::engine {

namespace configuration {
class file_info {
  uint32_t _line;
  std::string _path;

 public:
  file_info(std::string const& path = "", unsigned int line = 0)
      : _line(line), _path(path) {}
  file_info(file_info const& right) { operator=(right); }
  ~file_info() noexcept {}
  file_info& operator=(file_info const& right) {
    if (this != &right) {
      _line = right._line;
      _path = right._path;
    }
    return *this;
  }
  bool operator==(file_info const& right) const noexcept {
    return _line == right._line && _path == right._path;
  }
  bool operator!=(file_info const& right) const noexcept {
    return !operator==(right);
  }
  friend std::ostream& operator<<(std::ostream& s, file_info const& info) {
    s << "in file '" << info.path() << "' on line " << info.line();
    return s;
  }
  unsigned int line() const noexcept { return _line; }
  void line(unsigned int line) noexcept { _line = line; }
  std::string const& path() const noexcept { return _path; }
  void path(std::string const& path) { _path = path; }
};
}  // namespace configuration

}  // namespace com::centreon::engine

namespace fmt {
// formatter specializations for fmt
template <>
struct formatter<com::centreon::engine::configuration::file_info>
    : ostream_formatter {};
}  // namespace fmt

#endif  // !CCE_CONFIGURATION_FILE_INFO_HH
