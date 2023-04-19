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

#ifndef CCB_MULTIPLEXING_MUXER_FILTER_HH
#define CCB_MULTIPLEXING_MUXER_FILTER_HH

#include "bbdo/events.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace multiplexing {

constexpr unsigned max_filter_category =
    io::data_category::max_data_category + 1;

/**
 * @brief
 *
 * included internal
 */
class muxer_filter {
 protected:
  /**
   * @brief array that contains mask by category
   * the last element of the array represents internal category mask
   *
   */
  uint64_t _mask[max_filter_category];

 public:
  /**
   * @brief default constructor
   * accept all events by default
   */
  constexpr muxer_filter() : _mask{0xFFFFFFFFFFFFFFFF} {
    for (unsigned ind = 0; ind < max_filter_category; ++ind) {
      _mask[ind] = 0xFFFFFFFFFFFFFFFF;
    }
  }

  // the goal of this class is to force a zero initialisation
  struct zero_init {};

  /**
   * @brief constructor witch only initialize all to zero
   *
   */
  constexpr muxer_filter(const zero_init&) : _mask{0} {
    for (unsigned ind = 0; ind < max_filter_category; ++ind) {
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
        _mask[max_filter_category - 1] |= 1ULL << elem;
      } else {
        assert(cat + 1 < max_filter_category);
        _mask[cat] |= 1ULL << elem;
      }
    }
  }

  muxer_filter& operator|=(const muxer_filter& other) {
    const uint64_t* to_or = other._mask;
    for (uint64_t* to_fill = _mask; to_fill < _mask + max_filter_category;
         ++to_fill, ++to_or) {
      *to_fill |= *to_or;
    }
    return *this;
  }

  /**
   * @brief test if mess_type is allowed by this filter
   *
   * @param mess_type type of the event
   * @return true allowed
   * @return false not allowed
   */
  constexpr bool allowed(uint32_t mess_type) const {
    uint16_t cat = category_of_type(mess_type);
    uint16_t elem = element_of_type(mess_type);
    assert(cat == io::data_category::internal || cat + 1 < max_filter_category);
    if (cat == io::data_category::internal) {
      return _mask[max_filter_category - 1] & (1ULL << elem);
    } else {
      assert(cat + 1 < max_filter_category);
      return _mask[cat] & (1ULL << elem);
    }
  }
};

}  // namespace multiplexing

CCB_END()

#endif
