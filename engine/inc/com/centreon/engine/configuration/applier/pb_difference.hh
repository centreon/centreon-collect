/*
** Copyright 2023 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCE_CONFIGURATION_APPLIER_PB_DIFFERENCE_HH
#define CCE_CONFIGURATION_APPLIER_PB_DIFFERENCE_HH

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <google/protobuf/util/message_differencer.h>
#include <iterator>
#include "com/centreon/engine/namespace.hh"

CCE_BEGIN()

using MessageDifferencer = ::google::protobuf::util::MessageDifferencer;

namespace configuration {
namespace applier {
template <typename T,
          typename Container = ::google::protobuf::RepeatedPtrField<const T>>
class pb_difference {
  // What are the new objects
  std::vector<T> _added;
  // What index to delete
  std::vector<ssize_t> _deleted;
  // A vector of pairs, the pointer to the old one and the new one.
  std::vector<std::pair<T*, T>> _modified;

 public:
  /**
   * @brief Default constructor.
   */
  pb_difference() = default;

  /**
   * @brief Destructor.
   */
  ~pb_difference() noexcept = default;
  pb_difference(const pb_difference&) = delete;
  pb_difference& operator=(const pb_difference&) = delete;
  const std::vector<T>& added() const noexcept { return _added; }
  const std::vector<ssize_t>& deleted() const noexcept { return _deleted; }
  const std::vector<std::pair<T*, T>>& modified() const noexcept {
    return _modified;
  }

  template <typename TF>
  void parse(typename Container::iterator old_first,
             typename Container::iterator old_last,
             typename Container::iterator new_first,
             typename Container::iterator new_last,
             const std::function<TF(const T&)>& f) {
    absl::flat_hash_map<TF, T*> keys_values;
    for (auto it = old_first; it != old_last; ++it) {
      const T& item = *it;
      keys_values[f(item)] = const_cast<T*>(&(*it));
    }

    absl::flat_hash_set<TF> new_keys;
    for (auto it = new_first; it != new_last; ++it) {
      const T& item = *it;
      new_keys.insert(f(item));
      if (!keys_values.contains(f(item))) {
        // New object to add
        _added.push_back(item);
      } else {
        // Object to modify or equal
        if (!MessageDifferencer::Equals(item, *keys_values[f(item)])) {
          // There are changes in this object
          _modified.push_back(std::make_pair(keys_values[f(item)], *it));
        }
      }
    }

    ssize_t i = 0;
    for (auto it = old_first; it != old_last; ++it) {
      const T& item = *it;
      if (!new_keys.contains(f(item)))
        _deleted.push_back(i);
      ++i;
    }
  }

  void parse(typename Container::iterator old_first,
             typename Container::iterator old_last,
             typename Container::iterator new_first,
             typename Container::iterator new_last,
             const std::string& (T::*key)() const) {
    absl::flat_hash_map<std::string, T*> keys_values;
    for (auto it = old_first; it != old_last; ++it) {
      const T& item = *it;
      keys_values[(item.*key)()] = const_cast<T*>(&(*it));
    }

    absl::flat_hash_set<std::string> new_keys;
    for (auto it = new_first; it != new_last; ++it) {
      const T& item = *it;
      new_keys.insert((item.*key)());
      if (!keys_values.contains((item.*key)())) {
        // New object to add
        _added.push_back(item);
      } else {
        // Object to modify or equal
        if (!MessageDifferencer::Equals(item, *keys_values[(item.*key)()])) {
          // There are changes in this object
          _modified.push_back(std::make_pair(keys_values[(item.*key)()], *it));
        }
      }
    }

    ssize_t i = 0;
    for (auto it = old_first; it != old_last; ++it) {
      const T& item = *it;
      if (!new_keys.contains((item.*key)()))
        _deleted.push_back(i);
      ++i;
    }
  }
};
}  // namespace applier
}  // namespace configuration

CCE_END()

#endif  // !CCE_CONFIGURATION_APPLIER_PB_DIFFERENCE_HH
