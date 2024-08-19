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
