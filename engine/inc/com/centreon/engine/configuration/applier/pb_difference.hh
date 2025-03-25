/**
 * Copyright 2023-2025 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#ifndef CCE_CONFIGURATION_APPLIER_PB_DIFFERENCE_HH
#define CCE_CONFIGURATION_APPLIER_PB_DIFFERENCE_HH

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <google/protobuf/util/message_differencer.h>

namespace com::centreon::engine {

using MessageDifferencer = ::google::protobuf::util::MessageDifferencer;

namespace configuration::applier {
/**
 * @brief This class computes the difference between two "lists" of Protobuf
 * configuration objects. They are not really lists but RepeatedPtrFields or
 * similar things.
 *
 * When the class is instantiated, we can then call the parse() method to
 * compare two lists, for example the older one and the new one.
 *
 * The result is composed of three attributes:
 * * _added : objects that are in the new list and not in the old one.
 * * _deleted: objects in the old list but not in the new one.
 * * _modified: objects that changed from the old list to the new one.
 *
 * @tparam T The Protobuf type to compare with pb_difference, for example
 * configuration::Host, configuration::Service, etc...
 * @tparam Key The key type used to store these objects, for example an
 * std::string, an integer, etc...
 * @tparam Container The container type used to store the objects, by default a
 * RepeatedPtrField.
 */
template <typename T,
          typename Key,
          typename Container = ::google::protobuf::RepeatedPtrField<T>>
class pb_difference {
  // What are the new objects
  std::vector<const T*> _added;
  // What index to delete
  std::vector<std::pair<ssize_t, Key>> _deleted;
  // A vector of pairs, the pointer to the old one and the new one.
  std::vector<std::pair<T*, const T*>> _modified;

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
  const std::vector<const T*>& added() const noexcept { return _added; }
  const std::vector<std::pair<ssize_t, Key>>& deleted() const noexcept {
    return _deleted;
  }
  const std::vector<std::pair<T*, const T*>>& modified() const noexcept {
    return _modified;
  }

  /**
   * @brief The main function of pb_difference. It takes two iterators of the
   * old list, two iterators of the new one, and also a function giving the key
   * to recognize it. The function usually is connector_name(),
   * timeperiod_name(),... but it can also be a lambda returning a pair of IDs
   * (for example in the case of services).
   * The key returned by this last function is important since two different
   * objects with the same key represent a modification.
   *
   * @tparam Function
   * @param old_list the container of the current object configurations,
   * @param new_list the container of the new object configurations,
   * @param f The function returning the key of each object.
   */
  template <typename Function>
  void parse(Container& old_list, const Container& new_list, Function f) {
    absl::flat_hash_map<Key, T*> keys_values;
    for (auto it = old_list.begin(); it != old_list.end(); ++it) {
      T& item = *it;
      static_assert(std::is_same<decltype(f(item)), const Key&>::value ||
                        std::is_same<decltype(f(item)), const Key>::value ||
                        std::is_same<decltype(f(item)), Key>::value,
                    "Invalid key function: it must match Key");
      keys_values[f(item)] = &item;
    }

    absl::flat_hash_set<Key> new_keys;
    for (auto it = new_list.begin(); it != new_list.end(); ++it) {
      const T& item = *it;
      auto inserted = new_keys.insert(f(item));
      if (!keys_values.contains(*inserted.first)) {
        // New object to add
        _added.push_back(&item);
      } else {
        // Object to modify or equal
        if (!MessageDifferencer::Equals(item, *keys_values[f(item)])) {
          // There are changes in this object
          _modified.push_back(std::make_pair(keys_values[f(item)], &item));
        }
      }
    }

    ssize_t i = 0;
    for (auto it = old_list.begin(); it != old_list.end(); ++it) {
      const T& item = *it;
      if (!new_keys.contains(f(item)))
        _deleted.push_back({i, f(item)});
      ++i;
    }
  }

  void parse(const absl::flat_hash_map<Key, std::unique_ptr<T>>& old_content,
             const absl::flat_hash_map<Key, std::unique_ptr<T>>& new_content) {
    absl::flat_hash_set<Key> new_keys;
    for (auto& p : new_content) {
      if (!old_content.contains(p.first)) {
        // New object to add
        _added.push_back(p.second.get());
      } else {
        // Object to modify or equal
        if (!MessageDifferencer::Equals(*p.second, *old_content.at(p.first))) {
          // There are changes in this object
          _modified.push_back(
              std::make_pair(old_content.at(p.first).get(), p.second.get()));
        }
      }
      new_keys.insert(p.first);
    }

    ssize_t i = 0;
    for (auto& [k, _] : old_content) {
      if (!new_keys.contains(k))
        _deleted.push_back({i, k});
      ++i;
    }
  }

  void parse(Container& old_list,
             const Container& new_list,
             Key (T::*key)() const) {
    std::function<Key(const T&)> f = key;
    parse<std::function<Key(const T&)>>(old_list, new_list, f);
  }

  void parse(Container& old_list,
             const Container& new_list,
             const Key& (T::*key)() const) {
    std::function<const Key&(const T&)> f = key;
    parse<std::function<const Key&(const T&)>>(old_list, new_list, f);
  }
};
}  // namespace configuration::applier

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_PB_DIFFERENCE_HH
