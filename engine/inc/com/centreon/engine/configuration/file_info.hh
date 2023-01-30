/*
** Copyright 2011-2013 Merethis
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

#ifndef CCE_CONFIGURATION_FILE_INFO_HH
#define CCE_CONFIGURATION_FILE_INFO_HH

#include "com/centreon/engine/exceptions/error.hh"

CCE_BEGIN()

namespace configuration {
class file_info {
  uint32_t _line;
  std::string _path;

 public:
  file_info(const std::string& path, uint32_t line)
      : _line(line), _path(path) {}
  file_info(file_info&& other)
      : _line{other._line}, _path{std::move(other._path)} {}
  ~file_info() noexcept = default;
  file_info(const file_info&) = delete;
  file_info& operator=(const file_info&) = delete;
  file_info& operator=(file_info&& other) {
    if (this != &other) {
      _line = other._line;
      _path = std::move(other._path);
    }
    return *this;
  }

  friend exceptions::error& operator<<(exceptions::error& err,
                                       file_info const& info) {
    err << "in file '" << info.path() << "' on line " << info.line();
    return err;
  }
  unsigned int line() const noexcept { return _line; }
  const std::string& path() const noexcept { return _path; }
};
}  // namespace configuration

CCE_END()

#endif  // !CCE_CONFIGURATION_FILE_INFO_HH
