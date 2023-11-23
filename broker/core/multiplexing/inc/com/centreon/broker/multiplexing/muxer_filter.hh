/**
 * Copyright 2023 Centreon
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

#ifndef CCB_MULTIPLEXING_MUXER_FILTER_HH
#define CCB_MULTIPLEXING_MUXER_FILTER_HH

#include <cassert>
#include "bbdo/events.hh"

namespace com::centreon::broker {

namespace multiplexing {

namespace detail {
constexpr uint64_t all_events = 0xFFFFFFFFFFFFFFFF;
};

constexpr unsigned max_filter_category = io::data_category::max_data_category;

/**
 * @brief this class filters events according to their type
 * it uses a bit mask by event category, so it only supports a max of 63 events
 * by category it's constexpr constructible
 *
 */
class muxer_filter {
 protected:
  /**
   * @brief array that contains mask by category
   * the first element of the array represents internal category mask
   *
   */
  uint64_t _mask[max_filter_category];

 public:
  /**
   * @brief default constructor
   * accept all events by default
   */
  constexpr muxer_filter() : _mask{detail::all_events} {
    for (unsigned ind = 1; ind < max_filter_category; ++ind) {
      _mask[ind] = detail::all_events;
    }
  }

  // the goal of this class is to force a zero initialisation
  struct zero_init {};

  /**
   * @brief constructor witch only initialize all to zero
   *
   */
  constexpr muxer_filter(const zero_init&) : _mask{0} {
    for (unsigned ind = 1; ind < max_filter_category; ++ind) {
      _mask[ind] = 0;
    }
  }

  /**
   * @brief constructor
   * it allows event with mess_type type to be pushed
   * @param mess_types event type
   */
  constexpr muxer_filter(const std::initializer_list<uint32_t> mess_types)
      : _mask{0} {
    for (unsigned ind = 0; ind < max_filter_category; ++ind) {
      _mask[ind] = 0;
    }

    for (uint32_t mess_type : mess_types) {
      uint16_t cat = category_of_type(mess_type);
      uint16_t elem = element_of_type(mess_type);

      if (cat == io::data_category::internal) {
        _mask[0] |= 1ULL << elem;
      } else {
        assert(cat > 0 && cat < max_filter_category);
        _mask[cat] |= 1ULL << elem;
      }
    }
  }

  /**
   * @brief Add a new element type to the filter
   *
   * @element An element type
   */
  void insert(uint32_t element) {
    uint16_t cat = category_of_type(element);
    uint16_t elem = element_of_type(element);

    if (cat == io::data_category::internal) {
      _mask[0] |= 1ULL << elem;
    } else {
      assert(cat > 0 && cat < max_filter_category);
      _mask[cat] |= 1ULL << elem;
    }
  }

  template <typename const_uint32_iterator>
  constexpr muxer_filter(const_uint32_iterator begin, const_uint32_iterator end)
      : _mask{detail::all_events} {
    for (unsigned ind = 0; ind < max_filter_category; ++ind) {
      _mask[ind] = 0;
    }
    for (; begin != end; ++begin) {
      uint16_t cat = category_of_type(*begin);
      uint16_t elem = element_of_type(*begin);

      if (cat == io::data_category::internal) {
        _mask[0] |= 1ULL << elem;
      } else {
        assert(cat > 0 && cat < max_filter_category);
        _mask[cat] |= 1ULL << elem;
      }
    }
  }

  /**
   * @brief permits to add filters by doing an or with the argument
   *
   * @param other
   * @return constexpr muxer_filter&
   */
  constexpr muxer_filter& operator|=(const muxer_filter& other) {
    const uint64_t* to_or = other._mask;
    for (uint64_t* to_fill = _mask; to_fill < _mask + max_filter_category;
         ++to_fill, ++to_or) {
      *to_fill |= *to_or;
    }
    return *this;
  }

  /**
   * @brief do a and with the filter passed as an argument
   *
   * @param other
   * @return const muxer_filter&
   */
  constexpr muxer_filter& operator&=(const muxer_filter& other) {
    const uint64_t* to_and = other._mask;
    for (uint64_t* to_fill = _mask; to_fill < _mask + max_filter_category;
         ++to_fill, ++to_and) {
      *to_fill &= *to_and;
    }
    return *this;
  }

  /**
   * @brief Equals operator.
   *
   * @param other
   * @return const muxer_filter&
   */
  constexpr bool operator==(const muxer_filter& other) {
    const uint64_t* other_mask = other._mask;
    for (uint64_t* to_compare = _mask; to_compare < _mask + max_filter_category;
         ++to_compare, ++other_mask) {
      if (*to_compare != *other_mask)
        return false;
    }
    return true;
  }

  /**
   * @brief test if all events allowed in *this are also allowed in other
   *
   * @param other
   * @return true  all events allowed in *this are also allowed in other
   * @return false
   */
  constexpr bool is_in(const muxer_filter& other) const {
    const uint64_t* to_test = other._mask;
    for (const uint64_t* cat = _mask; cat < _mask + max_filter_category;
         ++cat, ++to_test) {
      if ((*cat & *to_test) != *cat) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief return true if filter allows all events
   *
   * @return true
   * @return false
   */
  constexpr bool allows_all() const {
    for (const uint64_t* cat = _mask; cat < _mask + max_filter_category;
         ++cat) {
      if (*cat != detail::all_events) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief list all categories for witch at least one event is allowed
   *
   * @return std::string
   */
  std::string get_allowed_categories() const {
    std::string ret;
    unsigned cat_ind = 0;
    char sep = ' ';
    for (const uint64_t* cat = _mask; cat < _mask + max_filter_category;
         ++cat, ++cat_ind) {
      if (*cat) {
        ret.push_back(sep);
        if (cat_ind)
          ret += io::category_name(io::data_category(cat_ind));
        else
          ret += io::category_name(io::data_category::internal);
        sep = ',';
      }
    }
    return ret;
  }

  /**
   * @brief test if mess_type is allowed by this filter
   *
   * @param mess_type type of the event
   *
   * @return true if the type is allowed by the filter, false otherwise.
   */
  constexpr bool allows(uint32_t mess_type) const {
    uint16_t cat = category_of_type(mess_type);
    uint16_t elem = element_of_type(mess_type);
    assert(cat == io::data_category::internal ||
           (cat > 0 && cat < max_filter_category));
    if (cat == io::data_category::internal) {
      return _mask[0] & (1ULL << elem);
    } else {
      return _mask[cat] & (1ULL << elem);
    }
  }
};

}  // namespace multiplexing

}  // namespace com::centreon::broker

#endif
