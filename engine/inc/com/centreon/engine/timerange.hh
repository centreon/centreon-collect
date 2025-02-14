/**
 * Copyright 2011-2019 Centreon
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
#ifndef CCE_OBJECTS_TIMERANGE_HH
#define CCE_OBJECTS_TIMERANGE_HH

namespace com::centreon::engine {
class timerange {
  uint64_t _range_start;
  uint64_t _range_end;

 public:
  timerange(uint64_t start, uint64_t end);
  uint64_t get_range_start() const { return _range_start; };
  uint64_t get_range_end() const { return _range_end; };
  // static timerange_list timeranges;

  bool operator==(timerange const& obj) const {
    return _range_start == obj._range_start && _range_end == obj._range_end;
  };
  bool operator!=(timerange const& obj) const {
    return _range_start != obj._range_start || _range_end != obj._range_end;
  };
};

using timerange_list = std::list<timerange>;
using days_array = std::array<timerange_list, 7>;

std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::timerange const& obj);
std::ostream& operator<<(std::ostream& os, timerange_list const& obj);

}  // namespace com::centreon::engine

#endif  // !CCE_OBJECTS_TIMERANGE_HH
