/**
 * Copyright 2011-2013,2017-2023 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CCE_CONFIGURATION_GROUP_HH
#define CCE_CONFIGURATION_GROUP_HH

#include <list>
#include <set>
#include <string>
#include <utility>

typedef std::list<std::string> list_string;
typedef std::set<std::string> set_string;
typedef std::set<std::pair<std::string, std::string> > set_pair_string;

namespace com::centreon::engine::configuration {

template <typename T>
class group {
 public:
  group(bool inherit = false);
  group(group const& other);
  ~group() noexcept;
  group& operator=(group const& other);
  group& operator=(std::string const& other);
  group& operator+=(group const& other);
  bool operator==(group const& other) const noexcept;
  bool operator!=(group const& other) const noexcept;
  bool operator<(group const& other) const noexcept;
  T& operator*() noexcept { return (_data); }
  T const& operator*() const noexcept { return (_data); }
  T* operator->() noexcept { return (&_data); }
  T const* operator->() const noexcept { return (&_data); }
  T& get() noexcept { return (_data); }
  T const& get() const noexcept { return (_data); }
  bool is_inherit() const noexcept { return (_is_inherit); }
  void is_inherit(bool enable) noexcept { _is_inherit = enable; }
  bool is_set() const noexcept { return (_is_set); }
  void reset();

 private:
  T _data;
  bool _is_inherit;
  bool _is_null;
  bool _is_set;
};
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_GROUP_HH
