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
#include "com/centreon/engine/configuration/diff_state.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::engine::configuration;
using Descriptor = ::google::protobuf::Descriptor;
using FieldDescriptor = ::google::protobuf::FieldDescriptor;
using Message = ::google::protobuf::Message;
using MessageDifferencer = ::google::protobuf::util::MessageDifferencer;
using Reflection = ::google::protobuf::Reflection;
using SpecificField =
    ::google::protobuf::util::MessageDifferencer::SpecificField;
using com::centreon::common::log_v2::log_v2;

void diff_state::_build_path(
    const std::vector<MessageDifferencer::SpecificField>& field_path,
    Path* path) {
  auto logger = log_v2::instance().get(common::log_v2::log_v2_configuration);
  logger->error("Build path");
  for (size_t i = 0; i < field_path.size(); ++i) {
    const SpecificField& specific_field = field_path[i];
    const FieldDescriptor* field = specific_field.field;

    if (field != nullptr && field->name() == "value") {
      // check to see if this the value label of a map value.  If so, skip it
      // because it isn't meaningful
      if (i > 0 && field_path[i - 1].field->is_map())
        continue;
    }

    if (field != nullptr) {
      logger->error("=> name: {}", field->name());
      logger->error("=> number: {}", field->number());

      if (field->is_map()) {
        const Message* entry1 = specific_field.map_entry1;
        // const Message* entry2 = specific_field.map_entry2;
        if (entry1 != nullptr) {
          // NB: the map key is always the first field
          const FieldDescriptor* fd = entry1->GetDescriptor()->field(0);
          if (fd->cpp_type() == FieldDescriptor::CPPTYPE_STRING) {
            // Not using PrintFieldValueToString for strings to avoid extra
            // characters
            std::string key_string = entry1->GetReflection()->GetString(
                *entry1, entry1->GetDescriptor()->field(0));
            logger->error("=> map key '{}'", key_string);

            Key* k = path->add_key();
            k->set_i32(field->number());
            k = path->add_key();
            k->set_str(std::move(key_string));
          } else {
            /* Not implemented */
            assert(2 == 3);
          }
        }
      } else {
        logger->error("=> index {}", specific_field.index);
        Key* k = path->add_key();
        k->set_i32(field->number());
        k = path->add_key();
        k->set_i32(specific_field.index);
      }
    }
  }
}

void diff_state::_set_value(
    const Message& message,
    const std::vector<MessageDifferencer::SpecificField>& field_path,
    bool left_side,
    Value* value) {
  const SpecificField& specific_field = field_path.back();
  const FieldDescriptor* field = specific_field.field;
  auto logger = log_v2::instance().get(common::log_v2::log_v2_configuration);
  if (field != nullptr) {
    int index = left_side ? specific_field.index : specific_field.new_index;
    switch (field->cpp_type()) {
      case FieldDescriptor::CPPTYPE_MESSAGE: {
        const Reflection* reflection = message.GetReflection();
        const Message& field_message =
            field->is_repeated()
                ? reflection->GetRepeatedMessage(message, field, index)
                : reflection->GetMessage(message, field);
        const FieldDescriptor* fd = nullptr;

        if (field->is_map()) {
          fd = field_message.GetDescriptor()->field(1);
          if (fd->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
            const Message* msg =
                &field_message.GetReflection()->GetMessage(field_message, fd);
            const std::string& name = msg->GetDescriptor()->name();
            if (name == "Contact")
              *value->mutable_value_ct() = *static_cast<const Contact*>(msg);
            else {
              logger->error("name of message '{}' not managed", name);
              assert(1 == 20);
            }
          } else {
          }
        } else {
          const std::string& name = field_message.GetDescriptor()->name();
          if (name == "Timeperiod")
            *value->mutable_value_tp() =
                *static_cast<const Timeperiod*>(&field_message);
          else if (name == "Contact")
            *value->mutable_value_ct() =
                *static_cast<const Contact*>(&field_message);
          else {
            logger->error("name of message '{}' not managed", name);
            assert(2 == 20);
          }
        }
      } break;
      case FieldDescriptor::CPPTYPE_STRING: {
        const Reflection* reflection = message.GetReflection();
        const std::string field_str =
            field->is_repeated()
                ? reflection->GetRepeatedString(message, field, index)
                : reflection->GetString(message, field);
        value->set_value_str(std::move(field_str));
      } break;
    }
  } else {
    logger->error("unknown field not handled");
    assert(2 == 3);
  }
}

/**
 * @brief Called when a new message is added
 *
 * @param google::protobuf::Message not used
 * @param google::protobuf::Message new_message The new message to add
 * @param std::vector A vector with specific changes, not used.
 */
void diff_state::ReportAdded(
    const Message&,
    const Message& new_message,
    const std::vector<MessageDifferencer::SpecificField>& field_path) {
  auto logger = log_v2::instance().get(common::log_v2::log_v2_configuration);
  logger->error("Added message");
  PathWithValue* path = _dstate.add_to_add();
  _build_path(field_path, path->mutable_path());
  _set_value(new_message, field_path, false, path->mutable_val());
}

/**
 * @brief Called when an object is removed from the state configuration.
 *
 * @param google::protobuf::Message old_message The object that has been
 * removed.
 * @param google::protobuf::Message
 * @param std::vector
 */
void diff_state::ReportDeleted(
    const google::protobuf::Message& old_message,
    const google::protobuf::Message&,
    const std::vector<MessageDifferencer::SpecificField>& field_path) {
  auto logger = log_v2::instance().get(common::log_v2::log_v2_configuration);
  logger->error("Removed message");
  _build_path(field_path, _dstate.add_to_remove());
  const Descriptor* desc = old_message.GetDescriptor();
  logger->error("Old_message : {}", desc->name());
}

void diff_state::ReportModified(
    const Message& /* old_message */,
    const Message& new_message,
    const std::vector<MessageDifferencer::SpecificField>& field_path) {
  const MessageDifferencer::SpecificField& specific_field = field_path.back();
  const FieldDescriptor* field = specific_field.field;
  auto logger = log_v2::instance().get(common::log_v2::log_v2_configuration);

  // Any changes to the subfields have already been handled.
  if (field == nullptr && specific_field.unknown_field_type ==
                              ::google::protobuf::UnknownField::TYPE_GROUP)
    return;
  else if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE)
    return;

  logger->error("Modified message");
  PathWithValue* path = _dstate.add_to_modify();
  _build_path(field_path, path->mutable_path());
  _set_value(new_message, field_path, false, path->mutable_val());
}

/**
 * @brief
 *
 * @return
 */
const DiffState& diff_state::report() const {
  return _dstate;
}
