/**
 * Copyright 2011-2013 Merethis
 * Copyright 2024 Centreon
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
#ifndef CCC_OPT_HH
#define CCC_OPT_HH

namespace com::centreon::common {

/**
 * @brief This class is kept because already used in Engine. But please, do
 * not use it anymore, prefer std::optional that plays almost the same role.
 *
 * @tparam T
 */
template <typename T>
class opt {
  T _data;
  bool _is_set;

 public:
  opt() : _data{}, _is_set(false) {}
  opt(T const& right) : _data(right), _is_set(false) {}
  opt(opt const& right) : _data(right._data), _is_set(right._is_set) {}
  ~opt() noexcept {}
  T const& operator=(T const& right) {
    set(right);
    return _data;
  }
  opt& operator=(opt const& right) {
    _data = right._data;
    _is_set = right._is_set;
    return *this;
  }
  bool operator==(opt const& right) const noexcept {
    return _data == right._data;
  }
  bool operator!=(opt const& right) const noexcept {
    return !operator==(right);
  }
  bool operator<(opt const& right) const noexcept {
    return _data < right._data;
  }
  operator T const&() const noexcept { return (_data); }
  T& operator*() noexcept { return (_data); }
  T const& operator*() const noexcept { return (_data); }
  T* operator->() noexcept { return (&_data); }
  T const* operator->() const noexcept { return (&_data); }
  T& get() noexcept { return (_data); }
  T const& get() const noexcept { return (_data); }
  bool is_set() const noexcept { return (_is_set); }
  void reset() noexcept { _is_set = false; }
  void set(T const& right) {
    _data = right;
    _is_set = true;
  }
  void set(opt<T> const& right) {
    _data = right._data;
    _is_set = right._is_set;
  }
};

}  // namespace com::centreon::common

#endif  // !CCC_OPT_HH
