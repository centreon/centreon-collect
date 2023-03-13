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
#include <absl/strings/str_split.h>
#include "com/centreon/engine/configuration/severity.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"
#include "state-generated.pb.h"

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

using Message = ::google::protobuf::Message;

bool fill_pair_string_group(PairStringSet* grp, const absl::string_view& value);
void fill_string_group(StringList* grp, const absl::string_view& value);
void fill_string_group(StringSet* grp, const absl::string_view& value);
bool fill_host_notification_options(uint32_t* options,
                                    const absl::string_view& value);
bool fill_service_notification_options(uint32_t* options,
                                       const absl::string_view& value);

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
    state = 18,
  };

 private:
  const object_type _otype;
  Message* _obj;
  const absl::flat_hash_map<std::string, std::string> _correspondence;
  std::vector<bool> _modified_field;
  bool _resolved = false;

 public:
  message_helper(object_type otype,
                 Message* obj,
                 absl::flat_hash_map<std::string, std::string>&& correspondence,
                 size_t field_size)
      : _otype(otype),
        _obj(obj),
        _correspondence{
            std::forward<absl::flat_hash_map<std::string, std::string>>(
                correspondence)},
        _modified_field(field_size, false) {}
  message_helper() = delete;
  message_helper& operator=(const message_helper&) = delete;
  virtual ~message_helper() noexcept = default;
  const absl::flat_hash_map<std::string, std::string>& correspondence() const {
    return _correspondence;
  }
  object_type otype() const { return _otype; }
  Message* mut_obj() { return _obj; }
  const Message* obj() const { return _obj; }
  void set_obj(Message* obj) { _obj = obj; }
  void set_changed(int num) { _modified_field[num] = true; }
  bool changed(int num) const { return _modified_field[num]; }
  bool resolved() const { return _resolved; }
  void resolve() { _resolved = true; }

  virtual bool hook(const absl::string_view& key [[maybe_unused]],
                    const absl::string_view& value [[maybe_unused]]) {
    return false;
  }
  virtual void check_validity() const = 0;
  absl::string_view validate_key(const absl::string_view& key) const;
  virtual bool insert_customvariable(absl::string_view key,
                                     absl::string_view value);
};
}  // namespace configuration
}  // namespace engine
}  // namespace centreon
}  // namespace com
#endif /* !CCE_CONFIGURATION_MESSAGE_HELPER_HH */
