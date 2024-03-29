/*
** Copyright 2017 Centreon
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

#ifndef CCB_GENERATOR_RECEIVER_HH
#define CCB_GENERATOR_RECEIVER_HH


namespace com::centreon::broker {

namespace generator {
/**
 *  @class receiver receiver.hh "com/centreon/broker/generator/receiver.hh"
 *  @brief Receive generated events.
 *
 *  Receive and control events generated by senders.
 */
class receiver : public io::stream {
 public:
  receiver();
  ~receiver();
  bool read(std::shared_ptr<io::data>& d, time_t deadline);
  int write(std::shared_ptr<io::data> const& d);

 private:
  receiver(receiver const& other);
  receiver& operator=(receiver const& other);

  std::unordered_map<uint32_t, uint32_t> _last_numbers;
};
}  // namespace generator

}

#endif  // !CCB_GENERATOR_RECEIVER_HH
