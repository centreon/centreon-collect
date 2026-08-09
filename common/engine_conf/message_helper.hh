/**
 * Copyright 2022-2024 Centreon (https://www.centreon.com/)
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
#include <absl/container/flat_hash_set.h>
#include <google/protobuf/util/message_differencer.h>
#include "common/engine_conf/state.pb.h"

namespace com::centreon::engine::configuration {

/**
 * @brief Error counter, it contains two attributes, one for warnings and
 * another for errors.
 */
struct error_cnt {
  uint32_t config_warnings = 0;
  uint32_t config_errors = 0;
};

/**
 * @brief What the *other* pollers define, as the central Broker knows it from
 * its per-poller configuration store.
 *
 * It is empty whenever the validation runs without a global view — Engine
 * checking its own configuration at startup, or Broker checking a single
 * directory in isolation — and the validators then behave exactly as before.
 *
 * It never makes a configuration valid or invalid: an object missing from the
 * poller under validation is an error either way. It only lets a validator name
 * the poller that owns the object, which turns an unactionable "not defined
 * anywhere" into a diagnostic an administrator can do something about.
 */
struct foreign_objects {
  /* Host name to the id of the poller defining it. */
  absl::flat_hash_map<std::string_view, uint64_t> hosts;
  /* {host name, service description} to the id of the poller defining it. */
  absl::flat_hash_map<std::pair<std::string_view, std::string_view>, uint64_t>
      services;

  /* The id of the poller defining @a host, 0 when no other poller does. */
  uint64_t poller_of_host(std::string_view host) const {
    auto it = hosts.find(host);
    return it == hosts.end() ? 0 : it->second;
  }

  /* The id of the poller defining the service, 0 when no other poller does. */
  uint64_t poller_of_service(std::string_view host,
                             std::string_view description) const {
    auto it = services.find({host, description});
    return it == services.end() ? 0 : it->second;
  }
};

/**
 * @brief Why a dependency reaching over to another poller is refused, worded
 * after its kind. Both kinds are errors, but not for the same reason and the
 * distinction matters to whoever reads the diagnostic.
 *
 * An execution dependency suppresses the check itself and is evaluated by the
 * poller at scheduling time: it could never span two pollers. A notification
 * dependency is evaluated by Broker from its global cache, which resolves both
 * ends by id and would hold it happily — what it lacks is a home, since a
 * dependency object only exists inside the Engine configuration of one poller.
 * Hence "not supported" rather than "impossible".
 *
 * @param kind The kind of the dependency, always set after expand().
 */
inline std::string_view cross_poller_reason(DependencyKind kind) {
  return kind == DependencyKind::execution_dependency
             ? "an execution dependency is evaluated by the poller itself and "
               "cannot span two pollers"
             : "a notification dependency across pollers is not supported";
}

/* Forward declarations */
class command_helper;
class connector_helper;
class contact_helper;
class contactgroup_helper;
class host_helper;
class hostdependency_helper;
class hostescalation_helper;
class hostgroup_helper;
class service_helper;
class servicedependency_helper;
class serviceescalation_helper;
class servicegroup_helper;
class timeperiod_helper;
class anomalydetection_helper;
class severity_helper;
class tag_helper;
class state_helper;

using ::google::protobuf::Message;

bool fill_pair_string_group(PairStringSet* grp, const std::string_view& value);
bool fill_pair_string_group(PairStringSet* grp,
                            const std::string_view& key,
                            const std::string_view& value);
void fill_string_group(StringList* grp, const std::string_view& value);
void fill_string_group(StringSet* grp, const std::string_view& value);
bool fill_host_notification_options(uint16_t* options,
                                    const std::string_view& value);
bool fill_service_notification_options(uint16_t* options,
                                       const std::string_view& value);
bool name_contains_illegal_chars(std::string_view name,
                                 std::string_view illegal_chars);

/**
 * @brief The base message helper used by every helpers. It defines the common
 * methods.
 *
 */
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
    hostgroup = 8,
    service = 9,
    servicedependency = 10,
    serviceescalation = 11,
    servicegroup = 13,
    timeperiod = 14,
    anomalydetection = 15,
    severity = 16,
    tag = 17,
    state = 18,
    nb_types = 19,
  };

 private:
  const object_type _otype;
  Message* _obj;
  /*
   * The centengine cfg file allows several words for a same field. For example,
   * we can have hosts, host, hostnames, hostname for the 'hostname' field.
   * This map gives as value the field name corresponding to the name specified
   * in the cfg file (the key). */
  const absl::flat_hash_map<std::string, std::string> _correspondence;
  /*
   * _modified_field is a vector used for inheritance. An object can inherit
   * from another one. To apply the parent values, we must be sure this object
   * does not already change the field before. And we cannot use the protobuf
   * default values since configuration objects have their own default values.
   * So, the idea is:
   * 1. The protobuf object is created.
   * 2. Thankgs to the helper, its default values are set.
   * 3. _modified_field cases are all set to false.
   * 4. Fields are modified while the cfg file is read and _modified_field is
   * updated in consequence.
   * 5. We can replace unchanged fields with the parent values if needed.
   */
  std::vector<bool> _modified_field;

  /* When a configuration object is resolved, this flag is set to true. */
  bool _resolved = false;

 public:
  /**
   * @brief Constructor of message_helper.
   *
   * @param otype An object_type specifying the type of the configuration
   * object.
   * @param obj The Protobuf message associated to the helper.
   * @param correspondence The correspondence table (see the _correspondence
   * map description for more details).
   * @param field_size The number of fields in the protobuf message (needed to
   * initialize the _modified_field).
   */
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

  /**
   * @brief For several keys, the parser of objects has some particular
   *        behaviors. These behaviors are handled here.
   * @param key The key to parse.
   * @param value The value corresponding to the key
   *
   * @return True on success.
   */
  virtual bool hook(std::string_view key [[maybe_unused]],
                    std::string_view value [[maybe_unused]]) {
    return false;
  }
  virtual void check_validity(error_cnt& err [[maybe_unused]]) const {}
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
      default:
        break;
    }
    retval->_obj = obj;
    return retval;
  }
  bool set(const std::string_view& key, const std::string_view& value);
};
}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_MESSAGE_HELPER_HH */
