/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/misc/get_options.hh"

#include "host_serv_extractor.hh"

using namespace com::centreon::engine::modules::opentelemetry;

/**
 * @brief test if a host serv is allowed for this extractor
 *
 * @param host
 * @param service_description
 * @return true
 * @return false
 */
bool host_serv_extractor::is_allowed(
    const std::string& host,
    const std::string& service_description) const {
  return _host_serv_list->is_allowed(host, service_description);
}

namespace com::centreon::engine::modules::opentelemetry::options {

/**
 * @brief command line parser for host_serv_attributes_extractor_config
 *
 */
class host_serv_attributes_extractor_config_options
    : public com::centreon::misc::get_options {
 public:
  host_serv_attributes_extractor_config_options();
  void parse(const std::string& cmd_line) { _parse_arguments(cmd_line); }
};

host_serv_attributes_extractor_config_options::
    host_serv_attributes_extractor_config_options() {
  _add_argument("host_attribute", 'h',
                "Where to find host attribute allowed values: resource, scope, "
                "data_point",
                true, true, "data_point");
  _add_argument("host_key", 'i',
                "the key of attributes where we can find host name", true, true,
                "host");
  _add_argument(
      "service_attribute", 's',
      "Where to find service attribute allowed values: resource, scope, "
      "data_point",
      true, true, "data_point");
  _add_argument("service_key", 't',
                "the key of attributes where we can find the name of service",
                true, true, "service");
}

};  // namespace com::centreon::engine::modules::opentelemetry::options

std::shared_ptr<host_serv_extractor> host_serv_extractor::create(
    const std::string& command_line,
    const commands::otel::host_serv_list::pointer& host_serv_list) {
  // type of the converter is the first field
  size_t sep_pos = command_line.find(' ');
  std::string conf_type = sep_pos == std::string::npos
                              ? command_line
                              : command_line.substr(0, sep_pos);
  boost::trim(conf_type);
  std::string params =
      sep_pos == std::string::npos ? "" : command_line.substr(sep_pos + 1);

  boost::trim(params);

  if (conf_type.empty() || conf_type == "attributes") {
    return std::make_shared<host_serv_attributes_extractor>(params,
                                                            host_serv_list);
  } else {
    SPDLOG_LOGGER_ERROR(log_v2::otl(), "unknown converter type:{}", conf_type);
    throw exceptions::msg_fmt("unknown converter type:{}", conf_type);
  }
}

host_serv_attributes_extractor::host_serv_attributes_extractor(
    const std::string& command_line,
    const commands::otel::host_serv_list::pointer& host_serv_list)
    : host_serv_extractor(command_line, host_serv_list) {
  options::host_serv_attributes_extractor_config_options args;
  args.parse(command_line);

  auto parse_param =
      [&args](
          const std::string& attribute,
          const std::string& key) -> std::pair<attribute_owner, std::string> {
    const std::string& path = args.get_argument(attribute).get_value();
    attribute_owner owner;
    if (path == "resource") {
      owner = attribute_owner::resource;
    } else if (path == "scope") {
      owner = attribute_owner::scope;
    } else {
      owner = attribute_owner::data_point;
    }

    std::string ret_key = args.get_argument(key).get_value();
    return std::make_pair(owner, ret_key);
  };

  try {
    std::tie(_host_path, _host_key) = parse_param("host_attribute", "host_key");
  } catch (const std::exception&) {  // default configuration
    _host_path = attribute_owner::data_point;
    _host_key = "host";
  }
  try {
    std::tie(_serv_path, _serv_key) =
        parse_param("service_attribute", "service_key");
  } catch (const std::exception&) {  // default configuration
    _serv_path = attribute_owner::data_point;
    _serv_key = "service";
  }
}

host_serv_metric host_serv_attributes_extractor::extract_host_serv_metric(
    const data_point& data_pt) const {
  auto extract =
      [](const data_point& data_pt, attribute_owner owner,
         const std::string& key) -> absl::flat_hash_set<std::string_view> {
    absl::flat_hash_set<std::string_view> ret;
    const ::google::protobuf::RepeatedPtrField<
        ::opentelemetry::proto::common::v1::KeyValue>* attributes = nullptr;
    switch (owner) {
      case attribute_owner::data_point:
        attributes = &data_pt.get_data_point_attributes();
        break;
      case attribute_owner::scope:
        attributes = &data_pt.get_scope().attributes();
        break;
      case attribute_owner::resource:
        attributes = &data_pt.get_resource().attributes();
        break;
      default:
        return ret;
    }
    for (const auto& key_val : *attributes) {
      if (key_val.key() == key && key_val.value().has_string_value()) {
        ret.insert(key_val.value().string_value());
      }
    }
    return ret;
  };

  host_serv_metric ret;

  absl::flat_hash_set<std::string_view> hosts =
      extract(data_pt, _host_path, _host_key);

  if (!hosts.empty()) {
    absl::flat_hash_set<std::string_view> services =
        extract(data_pt, _serv_path, _serv_key);
    ret = is_allowed(hosts, services);
    if (!ret.host.empty()) {
      ret.metric = data_pt.get_metric().name();
    }
  }

  return ret;
}
