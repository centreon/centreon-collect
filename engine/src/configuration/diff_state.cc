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
using Descriptor = ::google::protobuf::Descriptor;
using FieldDescriptor = ::google::protobuf::FieldDescriptor;
using Message = ::google::protobuf::Message;
using MessageDifferencer = ::google::protobuf::util::MessageDifferencer;
using Reflection = ::google::protobuf::Reflection;
using SpecificField = ::google::protobuf::util::MessageDifferencer::SpecificField;

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
  log_v2::config()->error("Added message");
  PathWithValue* path = _dstate.add_to_add();
  _build_path(field_path, path->mutable_path());
      const Descriptor* desc = new_message.GetDescriptor();
      log_v2::config()->error("New_message : {}", desc->name());
//      const ::google::protobuf::Message& field_message =
//          field->is_repeated()
//              ? reflection->GetRepeatedMessage(new_message, field, index)
//              : reflection->GetMessage(new_message, field);
//  const MessageDifferencer::SpecificField& specific_field = field_path.back();
//  const ::google::protobuf::FieldDescriptor* field = specific_field.field;
//  if (field != nullptr) {
//    int index = specific_field.new_index;
//    if (field->cpp_type() ==
//        ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
//      const ::google::protobuf::Reflection* reflection =
//          new_message.GetReflection();
//      const ::google::protobuf::Message& field_message =
//          field->is_repeated()
//              ? reflection->GetRepeatedMessage(new_message, field, index)
//              : reflection->GetMessage(new_message, field);
//
//      const ::google::protobuf::Message* message_ptr = &field_message;
//      if (field->is_map()) {
//        const ::google::protobuf::Descriptor* desc =
//            field_message.GetDescriptor();
//        const ::google::protobuf::FieldDescriptor* fd = desc->field(1);
//        if (fd->cpp_type() ==
//            ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
//          reflection = field_message.GetReflection();
//          message_ptr = &reflection->GetMessage(field_message, fd);
//        } else
//          message_ptr = &reflection->GetRepeatedMessage(field_message, fd, -1);
//      }
//
//      const ::google::protobuf::Descriptor* desc = message_ptr->GetDescriptor();
//      const std::string& name = desc->name();
//      if (name == "Timeperiod") {
//        log_v2::config()->debug("Timeperiod added");
//        Timeperiod* tp = _dstate.mutable_dtimeperiods()->add_to_add();
//        tp->CopyFrom(static_cast<const Timeperiod&>(*message_ptr));
//      } else if (name == "Command") {
//        log_v2::config()->debug("Command added");
//        Command* cmd = _dstate.mutable_dcommands()->add_to_add();
//        cmd->CopyFrom(static_cast<const Command&>(*message_ptr));
//      } else if (name == "Connector") {
//        log_v2::config()->debug("Connector added");
//        Connector* ct = _dstate.mutable_dconnectors()->add_to_add();
//        ct->CopyFrom(static_cast<const Connector&>(*message_ptr));
//      } else if (name == "State") {
//        log_v2::config()->debug("State added: Should not arrive");
//      } else if (name == "Contact") {
//        const Contact& new_contact = static_cast<const Contact&>(*message_ptr);
//        log_v2::config()->debug("Contact '{}' added", new_contact.name());
//        Contact& nc = (*_dstate.mutable_dcontacts()
//                            ->mutable_to_add())[new_contact.name()];
//        nc.CopyFrom(new_contact);
//      } else {
//        log_v2::config()->error(
//            "Message '{}' not managed in diff_state: this is a bug.", name);
//      }
//    } else {
//      const ::google::protobuf::Reflection* refl = new_message.GetReflection();
//      if (field->is_repeated()) {
//        if (field->cpp_type() ==
//            ::google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
//          const Contact& new_contact = static_cast<const Contact&>(new_message);
//          log_v2::config()->debug(
//              "STRING '{}' at index {}",
//              refl->GetRepeatedString(new_message, field, index), index);
//          auto& v = (*_dstate.mutable_dcontacts()
//                          ->mutable_to_modify())[new_contact.name()];
//          Field* f = v.add_list();
//          f->set_id(index);
//          f->set_action(Field_Action_ADD);
//          f->set_value_str(refl->GetRepeatedString(new_message, field, index));
//        }
//      } else {
//        if (field->cpp_type() ==
//            ::google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
//          log_v2::config()->debug("STRING '{}'",
//                                  refl->GetString(new_message, field));
//        }
//      }
//    }
//  } else {
//    log_v2::config()->error("Added message with unknown field");
//  }
}

void diff_state::_build_path(
    const std::vector<MessageDifferencer::SpecificField>& field_path,
    Path* path) {
  log_v2::config()->error("Build path");
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
      log_v2::config()->error("=> name: {}", field->name());
      log_v2::config()->error("=> number: {}", field->number());

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
            log_v2::config()->error("=> map key '{}'", key_string);

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
        log_v2::config()->error("=> number {}", field->number());
        log_v2::config()->error("=> index {}", specific_field.index);
        Key* k = path->add_key();
        k->set_i32(field->number());
        k = path->add_key();
        k->set_i32(specific_field.index);
      }
    }
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
  log_v2::config()->error("Modified message");
  _build_path(field_path, _dstate.add_to_remove());
      const Descriptor* desc = old_message.GetDescriptor();
      log_v2::config()->error("Old_message : {}", desc->name());
  //  const MessageDifferencer::SpecificField& specific_field =
  //  field_path.back(); const ::google::protobuf::FieldDescriptor* field =
  //  specific_field.field; if (field != nullptr) {
  //    int index = specific_field.index;
  //    if (field->cpp_type() ==
  //        ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
  //      const ::google::protobuf::Reflection* reflection =
  //          old_message.GetReflection();
  //      const ::google::protobuf::Message& field_message =
  //          field->is_repeated()
  //              ? reflection->GetRepeatedMessage(old_message, field, index)
  //              : reflection->GetMessage(old_message, field);
  //
  //      const ::google::protobuf::Message* message_ptr = &field_message;
  //      if (field->is_map()) {
  //        const ::google::protobuf::Descriptor* desc =
  //            field_message.GetDescriptor();
  //        const ::google::protobuf::FieldDescriptor* fd = desc->field(1);
  //        if (fd->cpp_type() ==
  //            ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
  //          reflection = field_message.GetReflection();
  //          message_ptr = &reflection->GetMessage(field_message, fd);
  //        } else
  //          message_ptr = &reflection->GetRepeatedMessage(field_message, fd,
  //          -1);
  //      }
  //
  //      const ::google::protobuf::Descriptor* desc =
  //      message_ptr->GetDescriptor(); const std::string& name = desc->name();
  //      if (name == "Timeperiod") {
  //        log_v2::config()->debug("Timeperiod removed");
  //        Timeperiod* tp = _dstate.mutable_dtimeperiods()->add_to_remove();
  //        tp->CopyFrom(static_cast<const Timeperiod&>(*message_ptr));
  //      } else if (name == "Command") {
  //        log_v2::config()->debug("Command removed");
  //        Command* cmd = _dstate.mutable_dcommands()->add_to_remove();
  //        cmd->CopyFrom(static_cast<const Command&>(*message_ptr));
  //      } else if (name == "Connector") {
  //        log_v2::config()->debug("Connector removed");
  //        Connector* ct = _dstate.mutable_dconnectors()->add_to_remove();
  //        ct->CopyFrom(static_cast<const Connector&>(*message_ptr));
  //      } else if (name == "State") {
  //        log_v2::config()->debug("State removed: Should not arrive");
  //      } else if (name == "Contact") {
  //        const Contact& old_contact = static_cast<const
  //        Contact&>(*message_ptr); log_v2::config()->debug("Contact '{}'
  //        removed", old_contact.name());
  //        _dstate.mutable_dcontacts()->add_to_remove(old_contact.name());
  //      } else {
  //        log_v2::config()->error(
  //            "Message '{}' not managed in diff_state: this is a bug.", name);
  //      }
  //    } else {
  //      const ::google::protobuf::Reflection* refl =
  //      old_message.GetReflection(); if (field->is_repeated()) {
  //        if (field->cpp_type() ==
  //            ::google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
  //          const Contact& new_contact = static_cast<const
  //          Contact&>(old_message); log_v2::config()->debug(
  //              "STRING '{}' at index {}",
  //              refl->GetRepeatedString(old_message, field, index), index);
  //          auto& v = (*_dstate.mutable_dcontacts()
  //                          ->mutable_to_modify())[new_contact.name()];
  //          Field* f = v.add_list();
  //          f->set_id(index);
  //          f->set_action(Field_Action_DEL);
  //          f->set_value_str(refl->GetRepeatedString(old_message, field,
  //          index));
  //        }
  //      } else {
  //        if (field->cpp_type() ==
  //            ::google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
  //          log_v2::config()->debug("STRING '{}'",
  //                                  refl->GetString(old_message, field));
  //        }
  //      }
  //    }
  //  } else {
  //    log_v2::config()->error("Removed message with unknown field");
  //  }
}

void diff_state::ReportModified(
    const Message& old_message,
    const Message& new_message,
    const std::vector<MessageDifferencer::SpecificField>& field_path) {
  //const Message* current_message = &old_message;
  log_v2::config()->error("Modified message");
  PathWithPair* path = _dstate.add_to_modify();
  _build_path(field_path, path->mutable_path());
      const Descriptor* desc = old_message.GetDescriptor();
      log_v2::config()->error("Old_message : {}", desc->name());

  //  const MessageDifferencer::SpecificField& specific_field =
  //  field_path.back(); const ::google::protobuf::FieldDescriptor* field =
  //  specific_field.field; if (field != nullptr) {
  //    int old_index = specific_field.index;
  //    int new_index = specific_field.new_index;
  //    if (field->cpp_type() ==
  //        ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
  //      const ::google::protobuf::Reflection* reflection =
  //          old_message.GetReflection();
  //      const ::google::protobuf::Message* old_field_message;
  //      const ::google::protobuf::Message* new_field_message;
  //      if (field->is_repeated()) {
  //        old_field_message =
  //            &reflection->GetRepeatedMessage(old_message, field, old_index);
  //        new_field_message =
  //            &reflection->GetRepeatedMessage(new_message, field, new_index);
  //      } else {
  //        old_field_message = &reflection->GetMessage(old_message, field);
  //        new_field_message = &reflection->GetMessage(new_message, field);
  //      }
  //
  //      if (field->is_map()) {
  //        const ::google::protobuf::Descriptor* desc =
  //            old_field_message->GetDescriptor();
  //        const ::google::protobuf::FieldDescriptor* fd = desc->field(1);
  //        if (fd->cpp_type() ==
  //            ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
  //          reflection = old_field_message->GetReflection();
  //          old_field_message = &reflection->GetMessage(*old_field_message,
  //          fd);
  //        } else
  //          old_field_message =
  //          &reflection->GetRepeatedMessage(*old_field_message, fd, -1);
  //      }
  //
  //      const ::google::protobuf::Descriptor* desc =
  //      old_field_message->GetDescriptor(); const std::string& name =
  //      desc->name(); if (name == "Timeperiod") {
  //        log_v2::config()->debug("Timeperiod modified");
  //        Timeperiod* tp = _dstate.mutable_dtimeperiods()->add_to_remove();
  //        tp->CopyFrom(static_cast<const Timeperiod&>(*old_field_message));
  //      } else if (name == "Command") {
  //        log_v2::config()->debug("Command modified");
  //        Command* cmd = _dstate.mutable_dcommands()->add_to_remove();
  //        cmd->CopyFrom(static_cast<const Command&>(*old_field_message));
  //      } else if (name == "Connector") {
  //        log_v2::config()->debug("Connector modified");
  //        Connector* ct = _dstate.mutable_dconnectors()->add_to_remove();
  //        ct->CopyFrom(static_cast<const Connector&>(*old_field_message));
  //      } else if (name == "State") {
  //        log_v2::config()->debug("State modified: Should not arrive");
  //      } else if (name == "Contact") {
  //        const Contact& old_contact = static_cast<const
  //        Contact&>(*old_field_message); log_v2::config()->debug("Contact '{}'
  //        modified", old_contact.name());
  //        _dstate.mutable_dcontacts()->add_to_remove(old_contact.name());
  //      } else {
  //        log_v2::config()->error(
  //            "Message '{}' not managed in diff_state: this is a bug.", name);
  //      }
  //    } else {
  //      const ::google::protobuf::Reflection* refl =
  //      old_message.GetReflection(); if (field->is_repeated()) {
  //        if (field->cpp_type() ==
  //            ::google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
  //          const Contact& new_contact = static_cast<const
  //          Contact&>(old_message); log_v2::config()->debug(
  //              "STRING '{}' at index {}",
  //              refl->GetRepeatedString(old_message, field, old_index),
  //              old_index);
  //          auto& v = (*_dstate.mutable_dcontacts()
  //                          ->mutable_to_modify())[new_contact.name()];
  //          Field* f = v.add_list();
  //          f->set_id(old_index);
  //          f->set_action(Field_Action_DEL);
  //          f->set_value_str(refl->GetRepeatedString(old_message, field,
  //          old_index));
  //        }
  //      } else {
  //        auto it = field_path.begin();
  //        auto& f = it->field;
  //        if (f != nullptr) {
  //          if (field->cpp_type() ==
  //          ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
  //      const ::google::protobuf::Message* old_field_message;
  //      if (f->is_repeated()) {
  //        const ::google::protobuf::Reflection* reflection =
  //        old_message.GetReflection(); const ::google::protobuf::Message*
  //        field_message =
  //            &reflection->GetRepeatedMessage(old_message, f, old_index);
  //      } else {
  //        //old_field_message = &reflection->GetMessage(old_message, field);
  //        //new_field_message = &reflection->GetMessage(new_message, field);
  //      }
  //
  ////      if (field->is_map()) {
  ////        const ::google::protobuf::Descriptor* desc =
  ////            old_field_message->GetDescriptor();
  ////        const ::google::protobuf::FieldDescriptor* fd = desc->field(1);
  ////        if (fd->cpp_type() ==
  ////            ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
  ////          reflection = old_field_message->GetReflection();
  ////          old_field_message = &reflection->GetMessage(*old_field_message,
  ///fd); /        } else /          old_field_message =
  ///&reflection->GetRepeatedMessage(*old_field_message, fd, -1); /      }
  //      const ::google::protobuf::Descriptor* desc =
  //      old_field_message->GetDescriptor(); const std::string& name =
  //      desc->name();
  //
  //          }
  //        }
  //
  //        //FieldChanges* fc =
  //        (*_dstate.mutable_dtimeperiods()->mutable_to_modify())[first.index];
  //
  //        //log_v2::config()->error("index = {}, new_index = {}", f.index,
  //        f.new_index); if (field->cpp_type() ==
  //            ::google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
  //          log_v2::config()->debug("STRING '{}' -> '{}'",
  //                                  refl->GetString(old_message, field),
  //                                  refl->GetString(new_message, field));
  //          //FieldChanges* fc =
  //          (*_dstate.mutable_dtimeperiods()->mutable_to_modify())[first.index];
  //
  //        }
  //      }
  //    }
  //  } else {
  //    log_v2::config()->error("Modified message with unknown field");
  //  }
}

/**
 * @brief
 *
 * @return
 */
const DiffState& diff_state::report() const {
  return _dstate;
}

//void MessageDifferencer::StreamReporter::PrintPath(
//    const std::vector<SpecificField>& field_path,
//    bool left_side) {
//  for (size_t i = 0; i < field_path.size(); ++i) {
//    SpecificField specific_field = field_path[i];
//
//    if (specific_field.field != nullptr &&
//        specific_field.field->name() == "value") {
//      // check to see if this the value label of a map value.  If so, skip it
//      // because it isn't meaningful
//      if (i > 0 && field_path[i - 1].field->is_map()) {
//        continue;
//      }
//    }
//    if (i > 0) {
//      printer_->Print(".");
//    }
//    if (specific_field.field != NULL) {
//      if (specific_field.field->is_extension()) {
//        printer_->Print("($name$)", "name", specific_field.field->full_name());
//      } else {
//        printer_->PrintRaw(specific_field.field->name());
//      }
//
//      if (specific_field.field->is_map()) {
//        PrintMapKey(left_side, specific_field);
//        continue;
//      }
//    } else {
//      printer_->PrintRaw(StrCat(specific_field.unknown_field_number));
//    }
//    if (left_side && specific_field.index >= 0) {
//      printer_->Print("[$name$]", "name", StrCat(specific_field.index));
//    }
//    if (!left_side && specific_field.new_index >= 0) {
//      printer_->Print("[$name$]", "name", StrCat(specific_field.new_index));
//    }
//  }
//}
