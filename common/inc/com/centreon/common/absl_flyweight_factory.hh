/**
 * Copyright 2025 Centreon
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

#ifndef _COM_ABSL_FLYWEIGHT_FACTORY_HH_
#define _COM_ABSL_FLYWEIGHT_FACTORY_HH_

#include <boost/flyweight/factory_tag.hpp>
#include "absl/container/node_hash_set.h"

namespace com::centreon::common {

template <bool use_mutex>
class lock_policy;

template <>
class lock_policy<true> {
  absl::Mutex _mut;

 public:
  void lock() { _mut.Lock(); }
  void unlock() { _mut.Unlock(); }
};

template <>
class lock_policy<false> {
 public:
  void lock() {}
  void unlock() {}
};

/**
 * @brief The goal of this class is to use absl container in boost flyweight
 * library in order to improve performance How to use it:
 * @code {.c++}
 * boost::flyweight<std::string,
 *                 boost::flyweights::factory<com::centreon::common::absl_factory_specifier>,
 *                 boost::flyweights::tag<message_tag>> message;
 * @endcode
 *
 * @tparam entry_type
 * @tparam value_hash
 */
template <class entry_type, class key_type, bool use_mutex>
class absl_flyweight_factory {
  struct compare {
    bool operator()(const entry_type& left, const entry_type& right) const {
      return static_cast<const key_type&>(left) ==
             static_cast<const key_type&>(right);
    }
  };
  struct hash {
    size_t operator()(const entry_type& to_hash) const {
      return absl::Hash<key_type>()(static_cast<const key_type&>(to_hash));
    }
  };

  // Nor node_hash_set, nor flat_hash_set guaranties iterator stability.
  //  So we refer elements by their address and we need pointer stability so we
  //  don't use flat_hash_set
  using store_type = absl::node_hash_set<entry_type, hash, compare>;

  store_type _store;

  lock_policy<use_mutex> _mut;

 public:
  using handle_type = const entry_type*;

  handle_type insert(entry_type&& val) {
    std::lock_guard l(_mut);
    auto insert_res = _store.emplace(std::move(val));
    return &*insert_res.first;
  }

  void erase(handle_type to_erase) {
    std::lock_guard l(_mut);
    _store.erase(*to_erase);
  }

  const entry_type& entry(handle_type handle) { return *handle; }
};

template <bool use_mutex>
struct absl_factory : boost::flyweights::factory_marker {
  template <typename entry_type, typename key_type>
  struct apply {
    using type = absl_flyweight_factory<entry_type, key_type, use_mutex>;
  };
};

}  // namespace com::centreon::common

#endif
