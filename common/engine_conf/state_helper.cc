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
#include "common/engine_conf/state_helper.hh"
#include <google/protobuf/descriptor.h>
#include <rapidjson/rapidjson.h>
#include "com/centreon/engine/events/sched_info.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/engine_conf/contact_helper.hh"
#include "common/engine_conf/contactgroup_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/hostdependency_helper.hh"
#include "common/engine_conf/hostescalation_helper.hh"
#include "common/engine_conf/service_helper.hh"
#include "common/engine_conf/servicedependency_helper.hh"
#include "common/engine_conf/serviceescalation_helper.hh"
#include "common/engine_conf/servicegroup_helper.hh"
#include "common/engine_conf/severity_helper.hh"
#include "common/log_v2/log_v2.hh"

using com::centreon::common::log_v2::log_v2;
using com::centreon::exceptions::msg_fmt;
using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Reflection;

extern sched_info scheduling_info;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a State object.
 *
 * @param obj The State object on which this helper works. The helper is not the
 * owner of this object.
 */
state_helper::state_helper(State* obj)
    : message_helper(
          object_type::state,
          obj,
          {
              {"check_for_orphaned_hosts", "check_orphaned_hosts"},
              {"check_for_orphaned_services", "check_orphaned_services"},
              {"check_result_reaper_frequency", "check_reaper_interval"},
              {"illegal_macro_output_chars", "illegal_output_chars"},
              {"illegal_object_name_chars", "illegal_object_chars"},
              {"max_concurrent_checks", "max_parallel_service_checks"},
              {"rpc_port", "grpc_port"},
              {"service_interleave_factor", "service_interleave_factor_method"},
              {"service_reaper_frequency", "check_reaper_interval"},
              {"use_agressive_host_checking", "use_aggressive_host_checking"},
              {"use_regexp_matching", "use_regexp_matches"},
              {"xcddefault_comment_file", "comment_file"},
              {"xdddefault_downtime_file", "downtime_file"},
          },
          State::descriptor()->field_count()) {
  _init();
}

/**
 * @brief For several keys, the parser of State objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool state_helper::hook(std::string_view key, std::string_view value) {
  State* obj = static_cast<State*>(mut_obj());
  /* Since we use key to get back the good key value, it is faster to give key
   * by copy to the method. We avoid one key allocation... */
  key = validate_key(key);

  if (key.substr(0, 10) == "log_level_") {
    if (value == "off" || value == "critical" || value == "error" ||
        value == "err" || value == "warning" || value == "info" ||
        value == "debug" || value == "trace") {
      if (value == "err")
        value = "error";
      return set_global(key, value);
    } else
      throw msg_fmt(
          "Log level '{}' has value '{}' but it cannot be a different string "
          "than off, critical, error, err, warning, info, debug or trace",
          key, value);
  } else if (key == "date_format") {
    if (value == "euro")
      obj->set_date_format(DateType::euro);
    else if (value == "iso8601")
      obj->set_date_format(DateType::iso8601);
    else if (value == "strict-iso8601")
      obj->set_date_format(DateType::strict_iso8601);
    else if (value == "us")
      obj->set_date_format(DateType::us);
    else
      return false;
    return true;
  } else if (key == "host_inter_check_delay_method") {
    if (value == "n")
      obj->mutable_host_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_none);
    else if (value == "d")
      obj->mutable_host_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_dumb);
    else if (value == "s")
      obj->mutable_host_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_smart);
    else {
      obj->mutable_host_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_user);
      double user_value;
      if (!absl::SimpleAtod(value, &user_value) || user_value <= 0.0)
        throw msg_fmt(
            "Invalid value for host_inter_check_delay_method, must be one of "
            "'n' (none), 'd' (dumb), 's' (smart) or a stricly positive value "
            "({} provided)",
            user_value);
      obj->mutable_host_inter_check_delay_method()->set_user_value(user_value);
    }
    return true;
  } else if (key == "service_inter_check_delay_method") {
    if (value == "n")
      obj->mutable_service_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_none);
    else if (value == "d")
      obj->mutable_service_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_dumb);
    else if (value == "s")
      obj->mutable_service_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_smart);
    else {
      obj->mutable_service_inter_check_delay_method()->set_type(
          InterCheckDelay_IcdType_user);
      double user_value;
      if (!absl::SimpleAtod(value, &user_value) || user_value <= 0.0)
        throw msg_fmt(
            "Invalid value for service_inter_check_delay_method, must be one "
            "of 'n' (none), 'd' (dumb), 's' (smart) or a stricly positive "
            "value ({} provided)",
            user_value);
      obj->mutable_service_inter_check_delay_method()->set_user_value(
          user_value);
    }
    return true;
  } else if (key == "command_check_interval") {
    std::string_view v;
    if (value[value.size() - 1] == 's') {
      obj->set_command_check_interval_is_seconds(true);
      v = value.substr(0, value.size() - 1);
    } else {
      obj->set_command_check_interval_is_seconds(false);
      v = value;
    }
    int32_t res;
    if (absl::SimpleAtoi(v, &res)) {
      obj->set_command_check_interval(res);
      return true;
    } else {
      throw msg_fmt(
          "command_check_interval is an integer representing a duration "
          "between two consecutive external command checks. This number can be "
          "a number of 'time units' or a number of seconds. For the latter, "
          "you must append a 's' after the number: the current incorrect value "
          "is: '{}'",
          fmt::string_view(value.data(), value.size()));
      return false;
    }
  } else if (key == "service_interleave_factor_method") {
    if (value == "s")
      obj->mutable_service_interleave_factor_method()->set_type(
          InterleaveFactor_IFType_ilf_smart);
    else {
      obj->mutable_service_interleave_factor_method()->set_type(
          InterleaveFactor_IFType_ilf_user);
      int32_t res;
      if (!absl::SimpleAtoi(value, &res) || res < 1)
        res = 1;
      obj->mutable_service_interleave_factor_method()->set_user_value(res);
    }
    return true;
  } else if (key == "check_reaper_interval") {
    int32_t res;
    if (!absl::SimpleAtoi(value, &res) || res == 0)
      throw msg_fmt(
          "check_reaper_interval must be a strictly positive integer (current "
          "value '{}'",
          fmt::string_view(value.data(), value.size()));
    else
      obj->set_check_reaper_interval(res);
    return true;
  } else if (key == "event_broker_options") {
    if (value != "-1") {
      uint32_t res;
      if (absl::SimpleAtoi(value, &res))
        obj->set_event_broker_options(res);
      else
        throw msg_fmt(
            "event_broker_options must be a positive integer or '-1' and not "
            "'{}'",
            fmt::string_view(value.data(), value.size()));
    } else
      obj->set_event_broker_options(static_cast<uint32_t>(-1));
    return true;
  } else if (key == "auto_reschedule_checks" ||
             key == "auto_rescheduling_interval" ||
             key == "auto_rescheduling_window") {
    std::shared_ptr<spdlog::logger> logger =
        log_v2::instance().get(log_v2::CONFIG);
    SPDLOG_LOGGER_WARN(
        logger,
        "The option '{}' is no longer available. This option is deprecated.",
        key);
    return true;
  }
  return false;
}

/**
 * @brief Initializer of the State object, in other words set its default
 * values.
 */
void state_helper::_init() {
  State* obj = static_cast<State*>(mut_obj());
  obj->set_accept_passive_host_checks(true);
  obj->set_accept_passive_service_checks(true);
  obj->set_additional_freshness_latency(15);
  obj->set_admin_email("");
  obj->set_admin_pager("");
  obj->set_allow_empty_hostgroup_assignment(false);
  obj->set_cached_host_check_horizon(15);
  obj->set_cached_service_check_horizon(15);
  obj->set_check_external_commands(true);
  obj->set_check_host_freshness(false);
  obj->set_check_orphaned_hosts(true);
  obj->set_check_orphaned_services(true);
  obj->set_check_reaper_interval(10);
  obj->set_check_service_freshness(true);
  obj->set_command_check_interval(-1);
  obj->set_command_file(DEFAULT_COMMAND_FILE);
  obj->set_date_format(DateType::us);
  obj->set_debug_file(DEFAULT_DEBUG_FILE);
  obj->set_debug_level(0);
  obj->set_debug_verbosity(1);
  obj->set_enable_environment_macros(false);
  obj->set_enable_event_handlers(true);
  obj->set_enable_flap_detection(false);
  obj->set_enable_macros_filter(false);
  obj->set_enable_notifications(true);
  obj->set_enable_predictive_host_dependency_checks(true);
  obj->set_enable_predictive_service_dependency_checks(true);
  obj->set_event_broker_options(std::numeric_limits<uint32_t>::max());
  obj->set_event_handler_timeout(30);
  obj->set_execute_host_checks(true);
  obj->set_execute_service_checks(true);
  obj->set_external_command_buffer_slots(4096);
  obj->set_global_host_event_handler("");
  obj->set_global_service_event_handler("");
  obj->set_high_host_flap_threshold(30.0);
  obj->set_high_service_flap_threshold(30.0);
  obj->set_host_check_timeout(30);
  obj->set_host_freshness_check_interval(60);
  obj->mutable_host_inter_check_delay_method()->set_type(
      InterCheckDelay_IcdType_smart);
  obj->set_illegal_object_chars("");
  obj->set_illegal_output_chars("`~$&|'\"<>");
  obj->set_interval_length(60);
  obj->set_log_event_handlers(true);
  obj->set_log_external_commands(true);
  obj->set_log_file(DEFAULT_LOG_FILE);
  obj->set_log_host_retries(false);
  obj->set_log_notifications(true);
  obj->set_log_passive_checks(true);
  obj->set_log_pid(true);
  obj->set_log_service_retries(false);
  obj->set_low_host_flap_threshold(20.0);
  obj->set_low_service_flap_threshold(20.0);
  obj->set_max_debug_file_size(1000000);
  obj->set_max_host_check_spread(5);
  obj->set_max_log_file_size(0);
  obj->set_max_parallel_service_checks(0);
  obj->set_max_service_check_spread(5);
  obj->set_notification_timeout(30);
  obj->set_obsess_over_hosts(false);
  obj->set_obsess_over_services(false);
  obj->set_ochp_command("");
  obj->set_ochp_timeout(15);
  obj->set_ocsp_command("");
  obj->set_ocsp_timeout(15);
  obj->set_perfdata_timeout(5);
  obj->set_poller_name("unknown");
  obj->set_rpc_listen_address("localhost");
  obj->set_process_performance_data(false);
  obj->set_retained_contact_host_attribute_mask(0L);
  obj->set_retained_contact_service_attribute_mask(0L);
  obj->set_retained_host_attribute_mask(0L);
  obj->set_retained_process_host_attribute_mask(0L);
  obj->set_retain_state_information(true);
  obj->set_retention_scheduling_horizon(900);
  obj->set_retention_update_interval(60);
  obj->set_service_check_timeout(60);
  obj->set_service_freshness_check_interval(60);
  obj->mutable_service_inter_check_delay_method()->set_type(
      InterCheckDelay_IcdType_smart);
  obj->mutable_service_interleave_factor_method()->set_type(
      InterleaveFactor_IFType_ilf_smart);
  obj->set_sleep_time(0.5);
  obj->set_soft_state_dependencies(false);
  obj->set_state_retention_file(DEFAULT_RETENTION_FILE);
  obj->set_status_file(DEFAULT_STATUS_FILE);
  obj->set_status_update_interval(60);
  obj->set_time_change_threshold(900);
  obj->set_use_large_installation_tweaks(false);
  obj->set_instance_heartbeat_interval(30);
  obj->set_use_regexp_matches(false);
  obj->set_use_retained_program_state(true);
  obj->set_use_retained_scheduling_info(false);
  obj->set_use_setpgid(true);
  obj->set_use_syslog(true);
  obj->set_log_v2_enabled(true);
  obj->set_log_legacy_enabled(false);
  obj->set_log_v2_logger("file");
  obj->set_log_level_functions(LogLevel::error);
  obj->set_log_level_config(LogLevel::info);
  obj->set_log_level_events(LogLevel::info);
  obj->set_log_level_checks(LogLevel::info);
  obj->set_log_level_notifications(LogLevel::error);
  obj->set_log_level_eventbroker(LogLevel::error);
  obj->set_log_level_external_command(LogLevel::error);
  obj->set_log_level_commands(LogLevel::error);
  obj->set_log_level_downtimes(LogLevel::error);
  obj->set_log_level_comments(LogLevel::error);
  obj->set_log_level_macros(LogLevel::error);
  obj->set_log_level_process(LogLevel::info);
  obj->set_log_level_runtime(LogLevel::error);
  obj->set_use_timezone("");
  obj->set_use_true_regexp_matching(false);
}

/**
 * @brief Given the helper to a State protobuf message (so we also have access
 * to the message itself) and a key/value pair, this function searches the
 * field key and applies to it the value. As we work here on the State message,
 * that's the reason of the name of this function.
 *
 * @param helper The State helper.
 * @param key The field name to look for.
 * @param value The value to apply.
 *
 * @return True on success.
 */
bool state_helper::set_global(const std::string_view& key,
                              const std::string_view& value) {
  State* msg = static_cast<State*>(mut_obj());
  const Descriptor* desc = msg->GetDescriptor();
  const FieldDescriptor* f;
  const Reflection* refl;

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
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_INT32: {
      int32_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetInt32(static_cast<Message*>(msg), f, val);
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_UINT32: {
      uint32_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetUInt32(static_cast<Message*>(msg), f, val);
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_UINT64: {
      uint64_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetUInt64(static_cast<Message*>(msg), f, val);
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_FLOAT: {
      float val;
      if (absl::SimpleAtof(value, &val)) {
        refl->SetFloat(static_cast<Message*>(msg), f, val);
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
      return true;
    case FieldDescriptor::TYPE_ENUM: {
      auto* v = f->enum_type()->FindValueByName(
          std::string(value.data(), value.size()));
      if (v)
        refl->SetEnumValue(msg, f, v->number());
      else
        return false;
    } break;
    case FieldDescriptor::TYPE_MESSAGE:
      if (!f->is_repeated()) {
        Message* m = refl->MutableMessage(msg, f);
        const Descriptor* d = m->GetDescriptor();

        if (d && d->name() == "StringSet") {
          StringSet* set =
              static_cast<StringSet*>(refl->MutableMessage(msg, f));
          fill_string_group(set, value);
          return true;
        } else if (d && d->name() == "StringList") {
          StringList* lst =
              static_cast<StringList*>(refl->MutableMessage(msg, f));
          fill_string_group(lst, value);
          return true;
        }
      } else {
        assert(124 == 123);
      }
      break;
    default:
      return false;
  }
  return true;
}

bool state_helper::apply_extended_conf(
    const std::string& file_path,
    const rapidjson::Document& json_doc,
    const std::shared_ptr<spdlog::logger>& logger) {
  bool retval = true;
  for (rapidjson::Value::ConstMemberIterator member_iter =
           json_doc.MemberBegin();
       member_iter != json_doc.MemberEnd(); ++member_iter) {
    const std::string_view field_name = member_iter->name.GetString();
    try {
      switch (member_iter->value.GetType()) {
        case rapidjson::Type::kNumberType: {
          std::string value_str;
          if (member_iter->value.IsInt64()) {
            int64_t value = member_iter->value.GetInt64();
            value_str = fmt::to_string(value);
          } else if (member_iter->value.IsDouble()) {
            double value = member_iter->value.GetDouble();
            value_str = fmt::to_string(value);
          }
          set_global(field_name, value_str);
        } break;
        case rapidjson::Type::kStringType: {
          const std::string_view field_value = member_iter->value.GetString();
          set_global(field_name, field_value);
        } break;
        case rapidjson::Type::kFalseType:
          set_global(field_name, "false");
          break;
        case rapidjson::Type::kTrueType:
          set_global(field_name, "true");
          break;
        case rapidjson::Type::kNullType:
          set_global(field_name, "");
          break;
        default:
          logger->error(
              "The field '{}' in the file '{}' can not be converted as a "
              "string",
              field_name, file_path);
          retval = false;
      }
    } catch (const std::exception& e) {
      logger->error(
          "The field '{}' in the file '{}' can not be converted as a string",
          field_name, file_path);
      retval = false;
    }
  }
  return retval;
}

/**
 * @brief Expand configuration objects.
 *
 * @param pb_config The protobuf configuration state to expand.
 * @param err The error count object to update in case of errors.
 */
void state_helper::expand(configuration::error_cnt& err) {
  configuration::State& pb_config = *static_cast<State*>(mut_obj());

  absl::flat_hash_map<std::string_view, const configuration::Host*> m_host;
  for (auto& h : pb_config.hosts())
    m_host.emplace(h.host_name(), &h);

  absl::flat_hash_map<std::string_view, configuration::Contactgroup*>
      m_contactgroups;
  for (auto& cg : *pb_config.mutable_contactgroups())
    m_contactgroups.emplace(cg.contactgroup_name(), &cg);

  absl::flat_hash_map<std::string_view, configuration::Hostgroup*> m_hostgroups;
  for (auto& hg : *pb_config.mutable_hostgroups())
    m_hostgroups.emplace(hg.hostgroup_name(), &hg);

  absl::flat_hash_map<std::string_view, configuration::Servicegroup*>
      m_servicegroups;
  for (auto& sg : *pb_config.mutable_servicegroups())
    m_servicegroups.emplace(sg.servicegroup_name(), &sg);

  // Expand contacts
  contact_helper::expand(pb_config, err, m_contactgroups);
  // Expand contactgroups
  contactgroup_helper::expand(pb_config, err, m_contactgroups);
  // Expand hosts
  host_helper::expand(pb_config, err, m_hostgroups);
  // Expand services
  service_helper::expand(pb_config, err, m_host, m_servicegroups);

  // Expand servicegroups
  servicegroup_helper::expand(pb_config, err, m_servicegroups);

  // Expand hostdependencies.
  hostdependency_helper::expand(pb_config, err, m_hostgroups);
  // Expand servicedependencies.
  servicedependency_helper::expand(pb_config, err, m_hostgroups,
                                   m_servicegroups);

  // Expand hostescalations
  hostescalation_helper::expand(pb_config, err, m_hostgroups);
  // Expand serviceescalations
  serviceescalation_helper::expand(pb_config, err, m_hostgroups,
                                   m_servicegroups);
  // Expand custom variables
  state_helper::_expand_cv(pb_config);
}

void state_helper::_expand_cv(configuration::State& s) {
  absl::flat_hash_set<std::string_view> cvs;
  for (auto& cv : s.macros_filter().data())
    cvs.emplace(cv);

  // Browse all anomalydetections.
  for (auto& ad_cfg : *s.mutable_anomalydetections()) {
    // Should custom variables be sent to broker ?
    for (auto& cv : *ad_cfg.mutable_customvariables()) {
      if (!s.enable_macros_filter() || cvs.contains(cv.name()))
        cv.set_is_sent(true);
    }
  }
  // Browse all contacts.
  for (auto& c : *s.mutable_contacts()) {
    // Should custom variables be sent to broker ?
    for (auto& cv : *c.mutable_customvariables()) {
      if (!s.enable_macros_filter() || cvs.contains(cv.name()))
        cv.set_is_sent(true);
    }
  }
  // Browse all hosts.
  for (auto& host_cfg : *s.mutable_hosts()) {
    // Should custom variables be sent to broker ?
    for (auto& cv : *host_cfg.mutable_customvariables()) {
      if (!s.enable_macros_filter() || cvs.contains(cv.name()))
        cv.set_is_sent(true);
    }
  }
  // Browse all services.
  for (auto& service_cfg : *s.mutable_services()) {
    // Should custom variables be sent to broker ?
    for (auto& cv : *service_cfg.mutable_customvariables()) {
      if (!s.enable_macros_filter() || cvs.contains(cv.name()))
        cv.set_is_sent(true);
    }
  }
}

void state_helper::diff(const State& old_state,
                        const State& new_state,
                        const std::shared_ptr<spdlog::logger>& logger,
                        DiffState* result) {
  severity_helper::diff(old_state.severities(), new_state.severities(), logger,
                        result->mutable_severities());
}

}  // namespace com::centreon::engine::configuration
