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
#include "com/centreon/engine/log_v2.hh"

using namespace com::centreon::engine::configuration;
using namespace ::google::protobuf::util;

/**
 * @brief Called when a new message is added
 *
 * @param google::protobuf::Message not used
 * @param google::protobuf::Message new_message The new message to add
 * @param std::vector A vector with specific changes, not used.
 */
void diff_state::ReportAdded(
    const google::protobuf::Message&,
    const google::protobuf::Message& new_message,
    const std::vector<MessageDifferencer::SpecificField>& field_path) {
  const MessageDifferencer::SpecificField& specific_field = field_path.back();
  const ::google::protobuf::FieldDescriptor* field = specific_field.field;
  if (field != nullptr) {
    int index = specific_field.new_index;
    if (field->cpp_type() ==
        ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
      const ::google::protobuf::Reflection* reflection =
          new_message.GetReflection();
      const ::google::protobuf::Message& field_message =
          field->is_repeated()
              ? reflection->GetRepeatedMessage(new_message, field, index)
              : reflection->GetMessage(new_message, field);

      const ::google::protobuf::Message* message_ptr = &field_message;
      if (field->is_map()) {
        const ::google::protobuf::Descriptor* desc =
            field_message.GetDescriptor();
        const ::google::protobuf::FieldDescriptor* fd = desc->field(1);
        if (fd->cpp_type() ==
            ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
          reflection = field_message.GetReflection();
          message_ptr = &reflection->GetMessage(field_message, fd);
        } else
          message_ptr = &reflection->GetRepeatedMessage(field_message, fd, -1);
      }

      const ::google::protobuf::Descriptor* desc = message_ptr->GetDescriptor();
      const std::string& name = desc->name();
      if (name == "Timeperiod") {
        log_v2::config()->debug("Timeperiod added");
        Timeperiod* tp = _dstate.mutable_dtimeperiods()->add_to_add();
        tp->CopyFrom(static_cast<const Timeperiod&>(*message_ptr));
      } else if (name == "Command") {
        log_v2::config()->debug("Command added");
        Command* cmd = _dstate.mutable_dcommands()->add_to_add();
        cmd->CopyFrom(static_cast<const Command&>(*message_ptr));
      } else if (name == "Connector") {
        log_v2::config()->debug("Connector added");
        Connector* ct = _dstate.mutable_dconnectors()->add_to_add();
        ct->CopyFrom(static_cast<const Connector&>(*message_ptr));
      } else if (name == "State") {
        log_v2::config()->debug("State added: Should not arrive");
      } else if (name == "Contact") {
        const Contact& new_contact = static_cast<const Contact&>(*message_ptr);
        log_v2::config()->debug("Contact '{}' added", new_contact.name());
        Contact& nc = (*_dstate.mutable_dcontacts()
                            ->mutable_to_add())[new_contact.name()];
        nc.CopyFrom(new_contact);
      } else {
        log_v2::config()->error(
            "Message '{}' not managed in diff_state: this is a bug.", name);
      }
    } else {
      const ::google::protobuf::Reflection* refl = new_message.GetReflection();
      if (field->is_repeated()) {
        if (field->cpp_type() ==
            ::google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
          const Contact& new_contact = static_cast<const Contact&>(new_message);
          log_v2::config()->debug(
              "STRING '{}' at index {}",
              refl->GetRepeatedString(new_message, field, index), index);
          auto& v = (*_dstate.mutable_dcontacts()
                          ->mutable_to_modify())[new_contact.name()];
          Field* f = v.add_list();
          f->set_id(index);
          f->set_action(Field_Action_ADD);
          f->set_value_str(refl->GetRepeatedString(new_message, field, index));
        }
      } else {
        if (field->cpp_type() ==
            ::google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
          log_v2::config()->debug("STRING '{}'",
                                  refl->GetString(new_message, field));
        }
      }
    }
  } else {
    log_v2::config()->error("Removed message with unknown field");
  }
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
  const MessageDifferencer::SpecificField& specific_field = field_path.back();
  const ::google::protobuf::FieldDescriptor* field = specific_field.field;
  if (field != nullptr) {
    int index = specific_field.index;
    if (field->cpp_type() ==
        ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
      const ::google::protobuf::Reflection* reflection =
          old_message.GetReflection();
      const ::google::protobuf::Message& field_message =
          field->is_repeated()
              ? reflection->GetRepeatedMessage(old_message, field, index)
              : reflection->GetMessage(old_message, field);

      const ::google::protobuf::Message* message_ptr = &field_message;
      if (field->is_map()) {
        const ::google::protobuf::Descriptor* desc =
            field_message.GetDescriptor();
        const ::google::protobuf::FieldDescriptor* fd = desc->field(1);
        if (fd->cpp_type() ==
            ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
          reflection = field_message.GetReflection();
          message_ptr = &reflection->GetMessage(field_message, fd);
        } else
          message_ptr = &reflection->GetRepeatedMessage(field_message, fd, -1);
      }

      const ::google::protobuf::Descriptor* desc = message_ptr->GetDescriptor();
      const std::string& name = desc->name();
      if (name == "Timeperiod") {
        log_v2::config()->debug("Timeperiod removed");
        Timeperiod* tp = _dstate.mutable_dtimeperiods()->add_to_remove();
        tp->CopyFrom(static_cast<const Timeperiod&>(*message_ptr));
      } else if (name == "Command") {
        log_v2::config()->debug("Command removed");
        Command* cmd = _dstate.mutable_dcommands()->add_to_remove();
        cmd->CopyFrom(static_cast<const Command&>(*message_ptr));
      } else if (name == "Connector") {
        log_v2::config()->debug("Connector removed");
        Connector* ct = _dstate.mutable_dconnectors()->add_to_remove();
        ct->CopyFrom(static_cast<const Connector&>(*message_ptr));
      } else if (name == "State") {
        log_v2::config()->debug("State removed: Should not arrive");
      } else if (name == "Contact") {
        const Contact& old_contact = static_cast<const Contact&>(*message_ptr);
        log_v2::config()->debug("Contact '{}' removed", old_contact.name());
        _dstate.mutable_dcontacts()->add_to_remove(old_contact.name());
      } else {
        log_v2::config()->error(
            "Message '{}' not managed in diff_state: this is a bug.", name);
      }
    } else {
      log_v2::config()->error("In ReportDeleted, case not implemented");
      assert(1 == 0);
      // TextFormat::PrintFieldValueToString(old_message, field, index,
      // &output); printer_->PrintRaw(output);
    }
  } else {
    log_v2::config()->error("Removed message with unknown field");
  }
}

void diff_state::ReportModified(
    const google::protobuf::Message& old_message,
    const google::protobuf::Message&,
    const std::vector<MessageDifferencer::SpecificField>& field_path) {}
/**
 * @brief
 *
 * @return
 */
const DiffState& diff_state::report() const {
  return _dstate;
}
