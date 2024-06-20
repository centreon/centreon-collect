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

#include "com/centreon/engine/globals.hh"
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
  return _host_serv_list->contains(host, service_description);
}

std::shared_ptr<host_serv_extractor> host_serv_extractor::create(
    const std::string& command_line,
    const commands::otel::host_serv_list::pointer& host_serv_list) {
  static initialized_data_class<po::options_description> desc(
      [](po::options_description& desc) {
        desc.add_options()("extractor", po::value<std::string>(),
                           "extractor type");
      });

  try {
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(command_line))
                  .options(desc)
                  .allow_unregistered()
                  .run(),
              vm);
    if (!vm.count("extractor")) {
      throw exceptions::msg_fmt("extractor flag not found in {}", command_line);
    }
    std::string extractor_type = vm["extractor"].as<std::string>();
    if (extractor_type == "attributes") {
      return std::make_shared<host_serv_attributes_extractor>(command_line,
                                                              host_serv_list);
    } else {
      throw exceptions::msg_fmt("unknown extractor in {}", command_line);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(config_logger, "fail to parse {}: {}", command_line,
                        e.what());
    throw;
  }
}

/**
 * @brief Construct a new host serv attributes extractor::host serv attributes
 * extractor object
 *
 * @param command_line command line that contains options used by extractor
 * @param host_serv_list list that will be shared bu host_serv_extractor and
 * otel_connector
 */
host_serv_attributes_extractor::host_serv_attributes_extractor(
    const std::string& command_line,
    const commands::otel::host_serv_list::pointer& host_serv_list)
    : host_serv_extractor(command_line, host_serv_list) {
  static initialized_data_class<po::options_description> desc(
      [](po::options_description& desc) {
        desc.add_options()(
            "host_path", po::value<std::string>(),
            "where to find host name. Example:\n"
            "resource_metrics.scopeMetrics.metrics.dataPoints.attributes.host\n"
            "or\n"
            "resource_metrics.resource.attributes.host\n"
            "or\n"
            "resource_metrics.scope_metrics.scope.attributes.host");
        desc.add_options()(
            "service_path", po::value<std::string>(),
            "where to find service description. Example:\n"
            "resource_metrics.scope_metrics.data.data_points.attributes."
            "service\n"
            "or\n"
            "resource_metrics.resource.attributes.service\n"
            "or\n"
            "resource_metrics.scope_metrics.scope.attributes.service");
      });

  static auto parse_path = [](const std::string& path, attribute_owner& attr,
                              std::string& key) {
    static re2::RE2 path_extractor("(?i)\\.(\\w+)\\.attributes\\.([\\.\\w]+)");
    std::string sz_attr;
    if (!RE2::PartialMatch(path, path_extractor, &sz_attr, &key)) {
      throw exceptions::msg_fmt(
          "we expect a path like "
          "resource_metrics.scope_metrics.data.data_points.attributes.host not "
          "{}",
          path);
    }

    boost::to_lower(sz_attr);
    if (sz_attr == "resource") {
      attr = attribute_owner::resource;
    } else if (sz_attr == "scope") {
      attr = attribute_owner::scope;
    } else {
      attr = attribute_owner::otl_data_point;
    }
  };

  try {
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(command_line))
                  .options(desc)
                  .allow_unregistered()
                  .run(),
              vm);
    if (!vm.count("host_path")) {
      _host_path = attribute_owner::otl_data_point;
      _host_key = "host";
    } else {
      parse_path(vm["host_path"].as<std::string>(), _host_path, _host_key);
    }
    if (!vm.count("service_path")) {
      _serv_path = attribute_owner::otl_data_point;
      _serv_key = "service";
    } else {
      parse_path(vm["service_path"].as<std::string>(), _serv_path, _serv_key);
    }
  } catch (const std::exception& e) {
    std::ostringstream options_allowed;
    options_allowed << desc;
    SPDLOG_LOGGER_ERROR(config_logger,
                        "fail to parse {}: {}\nallowed options: {}",
                        command_line, e.what(), options_allowed.str());
    throw;
  }
}

/**
 * @brief extract host and service names from configured attribute type
 *
 * @param data_pt
 * @return host_serv_metric host attribute is empty if no expected attribute is
 * found
 */
host_serv_metric host_serv_attributes_extractor::extract_host_serv_metric(
    const otl_data_point& data_pt) const {
  auto extract =
      [](const otl_data_point& data_pt, attribute_owner owner,
         const std::string& key) -> absl::flat_hash_set<std::string_view> {
    absl::flat_hash_set<std::string_view> ret;
    const ::google::protobuf::RepeatedPtrField<
        ::opentelemetry::proto::common::v1::KeyValue>* attributes = nullptr;
    switch (owner) {
      case attribute_owner::otl_data_point:
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
