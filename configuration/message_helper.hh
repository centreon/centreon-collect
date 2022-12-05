/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CONFIGURATION_MESSAGE_HELPER_HH
#define CCE_CONFIGURATION_MESSAGE_HELPER_HH
#include <absl/container/flat_hash_map.h>
#include "configuration/state-generated.hh"

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

using Message = ::google::protobuf::Message;

class message_helper {
 public:
  enum object_type {
    command = 0,
    connector = 1,
    contact = 2,
    contactgroup = 3,
    host = 4,
    hostdependency = 5,
    hostescalation = 6,
    hostextinfo = 7,
    hostgroup = 8,
    service = 9,
    servicedependency = 10,
    serviceescalation = 11,
    serviceextinfo = 12,
    servicegroup = 13,
    timeperiod = 14,
    anomalydetection = 15,
    severity = 16,
    tag = 17,
  };

 private:
  const object_type _otype;
  Message* _obj;
  const absl::flat_hash_map<std::string, std::string> _correspondance;
  std::vector<bool> _modified_field;
  bool _resolved = false;

 public:
  message_helper(object_type otype,
                 Message* obj,
                 absl::flat_hash_map<std::string, std::string>&& correspondance,
                 size_t field_size)
      : _otype(otype),
        _obj(obj),
        _correspondance{std::move(correspondance)},
        _modified_field(field_size, false) {}
  const absl::flat_hash_map<std::string, std::string>& correspondance() const {
    return _correspondance;
  }
  object_type otype() const { return _otype; }
  Message* mut_obj() { return _obj; }
  void set_obj(Message* obj) { _obj = obj; }
  const Message* obj() const { return _obj; }
  void set_changed(int num) { _modified_field[num] = true; }
  bool changed(int num) const { return _modified_field[num]; }
  bool resolved() const { return _resolved; }
  void resolve() { _resolved = true; }
};
}  // namespace configuration
}  // namespace engine
}  // namespace centreon
}  // namespace com
#endif /* !CCE_CONFIGURATION_MESSAGE_HELPER_HH */
