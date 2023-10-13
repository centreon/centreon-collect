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
#include "common/configuration/state-generated.pb.h"

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

class command_helper;
class connector_helper;
class contact_helper;
class contactgroup_helper;
class host_helper;
class hostdependency_helper;
class hostescalation_helper;
class hostextinfo_helper;
class hostgroup_helper;
class service_helper;
class servicedependency_helper;
class serviceescalation_helper;
class serviceextinfo_helper;
class servicegroup_helper;
class timeperiod_helper;
class anomalydetection_helper;
class severity_helper;
class tag_helper;
class state_helper;

using Message = ::google::protobuf::Message;

bool fill_pair_string_group(PairStringSet* grp, const std::string_view& value);
bool fill_pair_string_group(PairStringSet* grp,
                            const std::string_view& key,
                            const std::string_view& value);
void fill_string_group(StringList* grp, const std::string_view& value);
void fill_string_group(StringSet* grp, const std::string_view& value);
bool fill_host_notification_options(uint32_t* options,
                                    const std::string_view& value);
bool fill_service_notification_options(uint32_t* options,
                                       const std::string_view& value);

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
  message_helper(const message_helper& other);
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

  virtual bool hook(std::string_view key [[maybe_unused]],
                    const std::string_view& value [[maybe_unused]]) {
    return false;
  }
  virtual void check_validity() const = 0;
  std::string_view validate_key(const std::string_view& key) const;
  virtual bool insert_customvariable(std::string_view key,
                                     std::string_view value);
  template <typename T>
  static std::unique_ptr<T> clone(const T& other, Message* obj) {
    std::unique_ptr<T> retval;
    switch (other._otype) {
      case command:
        retval = std::make_unique<command_helper>(
            static_cast<const command_helper&>(other));
        break;
      case connector:
        retval = std::make_unique<connector_helper>(
            static_cast<const connector_helper&>(other));
        break;
      case contact:
        retval = std::make_unique<contact_helper>(
            static_cast<const contact_helper&>(other));
        break;
      case contactgroup:
        retval = std::make_unique<contactgroup_helper>(
            static_cast<const contactgroup_helper&>(other));
        break;
      case host:
        retval = std::make_unique<host_helper>(
            static_cast<const host_helper&>(other));
        break;
      case hostdependency:
        retval = std::make_unique<hostdependency_helper>(
            static_cast<const hostdependency_helper&>(other));
        break;
      case hostescalation:
        retval = std::make_unique<hostescalation_helper>(
            static_cast<const hostescalation_helper&>(other));
        break;
      case hostextinfo:
        //            retval = std::make_unique<hostextinfo_helper>(
        //                static_cast<const hostextinfo_helper&>(other));
        break;
      case hostgroup:
        retval = std::make_unique<hostgroup_helper>(
            static_cast<const hostgroup_helper&>(other));
        break;
      case service:
        retval = std::make_unique<service_helper>(
            static_cast<const service_helper&>(other));
        break;
      case servicedependency:
        retval = std::make_unique<servicedependency_helper>(
            static_cast<const servicedependency_helper&>(other));
        break;
      case serviceescalation:
        retval = std::make_unique<serviceescalation_helper>(
            static_cast<const serviceescalation_helper&>(other));
        break;
      case serviceextinfo:
        //            retval = std::make_unique<serviceextinfo_helper>(
        //                static_cast<const serviceextinfo_helper&>(other));
        break;
      case servicegroup:
        retval = std::make_unique<servicegroup_helper>(
            static_cast<const servicegroup_helper&>(other));
        break;
      case timeperiod:
        retval = std::make_unique<timeperiod_helper>(
            static_cast<const timeperiod_helper&>(other));
        break;
      case anomalydetection:
        retval = std::make_unique<anomalydetection_helper>(
            static_cast<const anomalydetection_helper&>(other));
        break;
      case severity:
        retval = std::make_unique<severity_helper>(
            static_cast<const severity_helper&>(other));
        break;
      case tag:
        retval =
            std::make_unique<tag_helper>(static_cast<const tag_helper&>(other));
        break;
      case state:
        retval = std::make_unique<state_helper>(
            static_cast<const state_helper&>(other));
        break;
    }
    retval->_obj = obj;
    return retval;
  }
};
}  // namespace configuration
}  // namespace engine
}  // namespace centreon
}  // namespace com
#endif /* !CCE_CONFIGURATION_MESSAGE_HELPER_HH */
