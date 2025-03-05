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
#include "common/engine_conf/message_helper.hh"

using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Reflection;

namespace com::centreon::engine::configuration {

/**
 * @brief Copy constructor of the base helper class. It contains basic methods
 * to access/modify the message fields. This constructor is needed by the
 * parser.
 *
 * @param other A reference to the helper to copy.
 */
message_helper::message_helper(const message_helper& other)
    : _otype(other._otype),
      _obj(other._obj),
      _correspondence(other._correspondence),
      _modified_field(other._modified_field),
      _resolved(other._resolved) {}

/**
 * @brief Sugar function to fill a PairStringSet field in a Protobuf message
 * from a string. This function is used when cfg files are read.
 *
 * @param grp The field to fill.
 * @param value The value as string is a pair of strings seperated by a comma.
 *
 * @return A boolean that is True on success.
 */
bool fill_pair_string_group(PairStringSet* grp, const std::string_view& value) {
  auto arr = absl::StrSplit(value, ',');

  bool first = true;
  auto itfirst = arr.begin();
  if (itfirst == arr.end())
    return true;

  do {
    auto itsecond = itfirst;
    ++itsecond;
    if (itsecond == arr.end())
      return false;
    std::string_view v1 = absl::StripAsciiWhitespace(*itfirst);
    std::string_view v2 = absl::StripAsciiWhitespace(*itsecond);
    if (first) {
      if (v1[0] == '+') {
        grp->set_additive(true);
        v1 = v1.substr(1);
      }
      first = false;
    }
    bool found = false;
    for (auto& m : grp->data()) {
      if (*itfirst == m.first() && *itsecond == m.second()) {
        found = true;
        break;
      }
    }
    if (!found) {
      auto* p = grp->mutable_data()->Add();
      p->set_first(v1.data(), v1.size());
      p->set_second(v2.data(), v2.size());
    }
    itfirst = itsecond;
    ++itfirst;
  } while (itfirst != arr.end());
  return true;
}

/**
 * @brief Sugar function to fill a PairStringSet field in a Protobuf message
 * from two strings. This function is used when cfg files are read.
 *
 * @param grp The field to fill.
 * @param key The first value in the pair to fill.
 * @param value The second value in the pair to fill.
 *
 * @return A boolean that is True on success.
 */
bool fill_pair_string_group(PairStringSet* grp,
                            const std::string_view& key,
                            const std::string_view& value) {
  std::string_view v1 = absl::StripAsciiWhitespace(key);
  std::string_view v2 = absl::StripAsciiWhitespace(value);
  bool found = false;
  for (auto& m : grp->data()) {
    if (v1 == m.first() && v2 == m.second()) {
      found = true;
      break;
    }
  }
  if (!found) {
    auto* p = grp->mutable_data()->Add();
    p->set_first(v1.data(), v1.size());
    p->set_second(v2.data(), v2.size());
  }
  return true;
}

/**
 * @brief Sugar function to fill a StringSet field in a Protobuf message
 * from a string. This function is used when cfg files are read.
 *
 * @param grp The field to fill.
 * @param value The value as string.
 *
 * @return A boolean that is True on success.
 */
void fill_string_group(StringSet* grp, const std::string_view& value) {
  auto arr = absl::StrSplit(value, ',');
  bool first = true;
  for (std::string_view d : arr) {
    d = absl::StripAsciiWhitespace(d);
    if (first) {
      if (d[0] == '+') {
        grp->set_additive(true);
        d = d.substr(1);
      }
      first = false;
    }
    bool found = false;
    for (auto& v : grp->data()) {
      if (v == d) {
        found = true;
        break;
      }
    }
    if (!found)
      grp->add_data(d.data(), d.size());
  }
}

/**
 * @brief Sugar function to fill a StringList field in a Protobuf message
 * from a string. This function is used when cfg files are read.
 *
 * @param grp The field to fill.
 * @param value The value as string.
 *
 * @return A boolean that is True on success.
 */
void fill_string_group(StringList* grp, const std::string_view& value) {
  auto arr = absl::StrSplit(value, ',');
  bool first = true;
  for (std::string_view d : arr) {
    d = absl::StripAsciiWhitespace(d);
    if (first) {
      if (d[0] == '+') {
        grp->set_additive(true);
        d = d.substr(1);
      }
      first = false;
    }
    grp->add_data(d.data(), d.size());
  }
}

/**
 * @brief Parse host notification options as string and set an uint32_t to
 * the corresponding values.
 *
 * @param options A pointer to the uint32_t to set/
 * @param value A string of options seperated by a comma.
 *
 * @return True on success.
 */
bool fill_host_notification_options(uint16_t* options,
                                    const std::string_view& value) {
  uint16_t tmp_options = action_hst_none;
  auto arr = absl::StrSplit(value, ',');
  for (auto& v : arr) {
    std::string_view value = absl::StripAsciiWhitespace(v);
    if (value == "d" || value == "down")
      tmp_options |= action_hst_down;
    else if (value == "u" || value == "unreachable")
      tmp_options |= action_hst_unreachable;
    else if (value == "r" || value == "recovery")
      tmp_options |= action_hst_up;
    else if (value == "f" || value == "flapping")
      tmp_options |= action_hst_flapping;
    else if (value == "s" || value == "downtime")
      tmp_options |= action_hst_downtime;
    else if (value == "n" || value == "none")
      tmp_options = action_hst_none;
    else if (value == "a" || value == "all")
      tmp_options = action_hst_down | action_hst_unreachable | action_hst_up |
                    action_hst_flapping | action_hst_downtime;
    else
      return false;
  }
  *options = tmp_options;
  return true;
}

/**
 * @brief Parse host notification options as string and set an uint32_t to
 * the corresponding values.
 *
 * @param options A pointer to the uint32_t to set/
 * @param value A string of options seperated by a comma.
 *
 * @return True on success.
 */
bool fill_service_notification_options(uint16_t* options,
                                       const std::string_view& value) {
  uint16_t tmp_options = action_svc_none;
  auto arr = absl::StrSplit(value, ',');
  for (auto& v : arr) {
    std::string_view value = absl::StripAsciiWhitespace(v);
    if (value == "u" || value == "unknown")
      tmp_options |= action_svc_unknown;
    else if (value == "w" || value == "warning")
      tmp_options |= action_svc_warning;
    else if (value == "c" || value == "critical")
      tmp_options |= action_svc_critical;
    else if (value == "r" || value == "recovery")
      tmp_options |= action_svc_ok;
    else if (value == "f" || value == "flapping")
      tmp_options |= action_svc_flapping;
    else if (value == "s" || value == "downtime")
      tmp_options |= action_svc_downtime;
    else if (value == "n" || value == "none")
      tmp_options = action_svc_none;
    else if (value == "a" || value == "all")
      tmp_options = action_svc_unknown | action_svc_warning |
                    action_svc_critical | action_svc_ok | action_svc_flapping |
                    action_svc_downtime;
    else
      return false;
  }
  *options = tmp_options;
  return true;
}

/**
 * @brief In some Engine configuration objects, several keys are possible for a
 * same field. This function returns the good key to access the protobuf message
 * field from another one. For example, "hosts", "hostname" may design the same
 * field named hostname in the protobuf message. This function returns
 * "hostname" for "hosts" or "hostname".
 *
 * @param key The key to check.
 *
 * @return The key used in the message.
 */
std::string_view message_helper::validate_key(
    const std::string_view& key) const {
  std::string_view retval;
  auto it = _correspondence.find(key);
  if (it != _correspondence.end())
    retval = it->second;
  else
    retval = key;
  return retval;
}

/**
 * @brief This function does nothing but it is derived in several message
 * helpers to insert custom variables.
 *
 * @param [[maybe_unused]] The name of the customvariable.
 * @param [[maybe_unused]] Its value.
 *
 * @return True on success.
 */
bool message_helper::insert_customvariable(std::string_view key
                                           [[maybe_unused]],
                                           std::string_view value
                                           [[maybe_unused]]) {
  return false;
}

/**
 * @brief Set the value given as a string to the object referenced by the key.
 * If the key does not exist, the correspondence table may be used to find a
 * replacement of the key. The function converts the value to the appropriate
 * type.
 *
 * Another important point is that many configuration objects contain the Object
 * obj message (something like an inheritance). This message contains three
 * field names, name, use and register that are important for templating. If
 * keys are one of these names, the function tries to work directly with the obj
 * message.
 *
 * @param key The key to localize the object to set.
 * @param value The value as string that will be converted to the good type.
 *
 * @return true on success.
 */
bool message_helper::set(const std::string_view& key,
                         const std::string_view& value) {
  Message* msg = mut_obj();
  const Descriptor* desc = msg->GetDescriptor();
  const FieldDescriptor* f;
  const Reflection* refl;

  /* Cases  where we have to work on the obj Object (the parent object) */
  if (key == "name" || key == "register" || key == "use") {
    f = desc->FindFieldByName("obj");
    if (f) {
      refl = msg->GetReflection();
      Object* obj = static_cast<Object*>(refl->MutableMessage(msg, f));

      /* Optimization to avoid a new string comparaison */
      switch (key[0]) {
        case 'n':  // name
          obj->set_name(std::string(value.data(), value.size()));
          break;
        case 'r': {  // register
          bool value_b;
          if (!absl::SimpleAtob(value, &value_b))
            return false;
          else
            obj->set_register_(value_b);
        } break;
        case 'u': {  // use
          obj->mutable_use()->Clear();
          auto arr = absl::StrSplit(value, ',');
          for (auto& t : arr) {
            std::string v{absl::StripAsciiWhitespace(t)};
            obj->mutable_use()->Add(std::move(v));
          }
        } break;
      }
      return true;
    }
  }

  f = desc->FindFieldByName(std::string(key.data(), key.size()));
  if (f == nullptr) {
    auto it = correspondence().find(key);
    if (it != correspondence().end())
      f = desc->FindFieldByName(it->second);
    if (f == nullptr)
      return false;
  }
  refl = msg->GetReflection();
  switch (f->type()) {
    case FieldDescriptor::TYPE_BOOL: {
      bool val;
      if (absl::SimpleAtob(value, &val)) {
        refl->SetBool(static_cast<Message*>(msg), f, val);
        set_changed(f->index());
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_INT32: {
      int32_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetInt32(static_cast<Message*>(msg), f, val);
        set_changed(f->index());
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_UINT32: {
      uint32_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetUInt32(static_cast<Message*>(msg), f, val);
        set_changed(f->index());
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_UINT64: {
      uint64_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetUInt64(static_cast<Message*>(msg), f, val);
        set_changed(f->index());
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_DOUBLE: {
      double val;
      if (absl::SimpleAtod(value, &val)) {
        refl->SetDouble(static_cast<Message*>(msg), f, val);
        set_changed(f->index());
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_STRING:
      if (f->is_repeated()) {
        refl->AddString(static_cast<Message*>(msg), f,
                        std::string(value.data(), value.size()));
      } else {
        refl->SetString(static_cast<Message*>(msg), f,
                        std::string(value.data(), value.size()));
      }
      set_changed(f->index());
      return true;
    case FieldDescriptor::TYPE_MESSAGE:
      if (!f->is_repeated()) {
        Message* m = refl->MutableMessage(msg, f);
        const Descriptor* d = m->GetDescriptor();

        if (d && d->name() == "StringSet") {
          StringSet* set =
              static_cast<StringSet*>(refl->MutableMessage(msg, f));
          fill_string_group(set, value);
          set_changed(f->index());
          return true;
        } else if (d && d->name() == "StringList") {
          StringList* lst =
              static_cast<StringList*>(refl->MutableMessage(msg, f));
          fill_string_group(lst, value);
          set_changed(f->index());
          return true;
        }
      } else {
        assert(22 == 23);
      }
      break;
    case FieldDescriptor::TYPE_ENUM: {
      auto* v = f->enum_type()->FindValueByName(
          std::string(value.data(), value.size()));
      if (v)
        refl->SetEnumValue(msg, f, v->number());
      else
        return false;
    } break;
    default:
      return false;
  }
  return true;
}

}  // namespace com::centreon::engine::configuration
