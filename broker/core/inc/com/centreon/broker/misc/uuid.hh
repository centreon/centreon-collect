/*
** Copyright 2015 Centreon
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

#ifndef CCB_MISC_UUID_HH_
#define CCB_MISC_UUID_HH_

#include <stdint.h>

namespace com::centreon::broker {

namespace misc {

class Uuid {
 public:
  Uuid();
  Uuid(Uuid const& uuid);
  Uuid const& operator=(Uuid const& uuid);

  bool operator==(Uuid const& uuid) const;

  std::string to_string() const;

 private:
  uint32_t _time_low;
  uint16_t _time_mid;
  uint16_t _time_hi_and_version;
  uint8_t _clock_seq_hi_and_reserved;
  uint8_t _clock_seq_low;
  uint8_t _node[6];
};
}  // namespace misc

}

#endif  // CCB_MISC_UUID_HH_
