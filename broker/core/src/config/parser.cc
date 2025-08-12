/**
 * Copyright 2011-2013,2015,2017-2025 Centreon
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

#include "com/centreon/broker/config/parser.hh"

#include <syslog.h>

#include <absl/strings/match.h>
#include <absl/strings/str_split.h>

#include "com/centreon/broker/exceptions/deprecated.hh"
#include "com/centreon/broker/misc/filesystem.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::config;
using namespace nlohmann;

using msg_fmt = com::centreon::exceptions::msg_fmt;
using deprecated = com::centreon::broker::exceptions::deprecated;
using log_v2 = com::centreon::common::log_v2::log_v2;

template <typename T, typename U>
static bool get_conf(std::pair<std::string const, json> const& obj,
                     std::string key,
                     U& s,
                     void (U::*set_state)(T),
                     bool (json::*is_goodtype)() const,
                     T (json::*get_value)() const) {
  if (obj.first == key) {
    const json& value = obj.second;
    if ((value.*is_goodtype)())
      (s.*set_state)((value.*get_value)());
    else
      throw msg_fmt(
          "config parser: cannot parse key '{}': value type is invalid", key);
    return true;
  }
  return false;
}

template <typename U>
static bool get_conf(std::pair<std::string const, json> const& obj,
                     std::string key,
                     U& s,
                     void (U::*set_state)(const std::string&),
                     bool (json::*is_goodtype)() const) {
  if (obj.first == key) {
    const json& value = obj.second;
    if ((value.*is_goodtype)())
      (s.*set_state)(value.get<std::string>());
    else
      throw msg_fmt(
          "config parser: cannot parse key '{}': "
          "value type is invalid",
          key);
    ;
    return true;
  }
  return false;
}

/**
 * @brief Check if the json object elem contains an entry with the given key.
 *        If it contains it, it gets it and returns it as a boolean. In case of
 *        error, it throws an exception.
 *        If the key doesn't exist, nothing is returned, that's the reason we
 *        use absl::optional<>.
 *
 * @param elem The json object to work with.
 * @param key  the key to obtain the value.
 *
 * @return an absl::optional<bool> containing the wanted value or empty if
 *         nothing found.
 */
template <>
absl::optional<bool> parser::check_and_read<bool>(const nlohmann::json& elem,
                                                  const std::string& key) {
  if (elem.contains(key)) {
    auto& ret = elem[key];
    if (ret.is_boolean())
      return {ret};
    if (ret.is_number())
      return {ret};
    else if (ret.is_string()) {
      bool tmp;
      if (!absl::SimpleAtob(ret.get<std::string_view>(), &tmp))
        throw msg_fmt(
            "config parser: cannot parse key '{}': the string value must "
            "contain a boolean (1/0, yes/no, true/false)",
            key);
      return {tmp};
    } else
      throw msg_fmt(
          "config parser: cannot parse key '{}': the content must be a boolean "
          "(1/0, yes/no, true/false",
          key);
  }
  return absl::nullopt;
}

/**
 * @brief Check if the json object elem contains an entry with the given key.
 *        If it contains it, it gets it and returns it as a string. In case of
 *        error, it throws an exception.
 *        If the key doesn't exist, nothing is returned, that's the reason we
 *        use absl::optional<>.
 *
 * @param elem The json object to work with.
 * @param key  the key to obtain the value.
 *
 * @return an absl::optional<std::string> containing the wanted value or empty
 *         if nothing found.
 */
template <>
absl::optional<std::string> parser::check_and_read<std::string>(
    const nlohmann::json& elem,
    const std::string& key) {
  if (elem.contains(key)) {
    auto& el = elem[key];
    if (el.is_string())
      return {el};
    else
      throw msg_fmt(
          "config parser: cannot parse key '{}': the content must be a string",
          key);
  }
  return absl::nullopt;
}

/**
 *  Parse a configuration file.
 *
 *  @param[in]  file File to process.
 *
 *  @return a state corresponding to the json file processed.
 */
state parser::parse(std::string const& file) {
  state retval;
  // Parse JSON document.
  std::ifstream f(file);
  auto logger_config = log_v2::instance().get(log_v2::CONFIG);

  if (f.fail())
    throw msg_fmt("Config parser: Cannot read file '{}': {}", file,
                  std::strerror(errno));

  std::string const& json_to_parse{std::istreambuf_iterator<char>(f),
                                   std::istreambuf_iterator<char>()};
  std::string err;
  nlohmann::json json_document;

  try {
    json_document = json::parse(json_to_parse);
  } catch (const json::parse_error& e) {
    err = e.what();
  }

  if (json_document.is_null())
    throw msg_fmt("Config parser: Cannot parse file '{}': {}", file, err);

  try {
    if (json_document.is_object() &&
        json_document["centreonBroker"].is_object()) {
      std::string module;
      for (auto it = json_document["centreonBroker"].begin();
           it != json_document["centreonBroker"].end(); ++it) {
        if (it.key() == "command_file" && it.value().is_object())
          ;
        else if (get_conf<int, state>({it.key(), it.value()}, "broker_id",
                                      retval, &state::broker_id,
                                      &json::is_number, &json::get<int>))
          ;
        else if (it.key() == "grpc" && it.value().is_object()) {
          if (json_document["centreonBroker"]["grpc"].contains("rpc_port")) {
            if (json_document["centreonBroker"]["grpc"]["rpc_port"]
                    .is_number()) {
              retval.rpc_port(static_cast<uint16_t>(
                  json_document["centreonBroker"]["grpc"]["rpc_port"]
                      .get<int>()));
            } else
              throw msg_fmt(
                  "The rpc_port value in the grpc object should be an integer");
          }
          if (json_document["centreonBroker"]["grpc"].contains(
                  "listen_address")) {
            if (json_document["centreonBroker"]["grpc"]["listen_address"]
                    .is_string()) {
              retval.listen_address(
                  json_document["centreonBroker"]["grpc"]["listen_address"]
                      .get<std::string>());
            } else
              throw msg_fmt(
                  "The listen_address value in the grpc object should be a "
                  "string");
          }
        } else if (it.key() == "bbdo_version" && it.value().is_string()) {
          std::string version = json_document["centreonBroker"]["bbdo_version"]
                                    .get<std::string>();
          std::list<std::string_view> v = absl::StrSplit(version, '.');
          if (v.size() != 3)
            throw msg_fmt("config parser: cannot parse bbdo_version '{}'",
                          version);

          auto it = v.begin();
          uint32_t major;
          uint32_t minor;
          uint32_t patch;
          if (!absl::SimpleAtoi(*it, &major))
            throw msg_fmt(
                "config parser: cannot parse major value of bbdo_version '{}'",
                version);
          ++it;
          if (!absl::SimpleAtoi(*it, &minor))
            throw msg_fmt(
                "config parser: cannot parse minor value of bbdo_version '{}'",
                version);
          ++it;
          if (!absl::SimpleAtoi(*it, &patch))
            throw msg_fmt(
                "config parser: cannot parse patch value of bbdo_version '{}'",
                version);
          ++it;

          retval.set_bbdo_version(bbdo::bbdo_version(major, minor, patch));
        } else if (get_conf<state>(
                       {it.key(), it.value()}, "cache_config_directory", retval,
                       &state::set_cache_config_dir, &json::is_string))
          ;
	else if (get_conf<state>(
                       {it.key(), it.value()}, "pollers_config_directory", retval,
                       &state::set_pollers_config_dir, &json::is_string))
          ;
        else if (get_conf<state>({it.key(), it.value()}, "broker_name", retval,
                                 &state::broker_name, &json::is_string))
          ;
        else if (get_conf<int, state>({it.key(), it.value()}, "poller_id",
                                      retval, &state::poller_id,
                                      &json::is_number, &json::get<int>))
          ;
        else if (get_conf<state>({it.key(), it.value()}, "poller_name", retval,
                                 &state::poller_name, &json::is_string))
          ;
        else if (get_conf<state>({it.key(), it.value()}, "module_directory",
                                 retval, &state::module_directory,
                                 &json::is_string)) {
          if (!misc::filesystem::readable(retval.module_directory()))
            throw msg_fmt("The module directory '{}' is not accessible",
                          retval.module_directory());
        } else if (get_conf<state>({it.key(), it.value()}, "cache_directory",
                                   retval, &state::cache_directory,
                                   &json::is_string)) {
          if (!misc::filesystem::readable(retval.cache_directory()))
            throw msg_fmt("The cache directory '{}' is not accessible",
                          retval.cache_directory());
        } else if (get_conf<state>(
                       {it.key(), it.value()}, "cache_config_directory", retval,
                       &state::set_cache_config_dir, &json::is_string)) {
          if (!misc::filesystem::readable(retval.cache_config_dir()))
            throw msg_fmt(
                "The cache configuration directory '{}' is not accessible",
                retval.cache_config_dir());
        } else if (get_conf<state>({it.key(), it.value()},
                                   "pollers_config_directory", retval,
                                   &state::set_pollers_config_dir,
                                   &json::is_string)) {
          if (!misc::filesystem::readable(retval.pollers_config_dir()))
            throw msg_fmt(
                "The pollers configuration directory '{}' is not accessible",
                retval.pollers_config_dir());
        } else if (get_conf<int, state>({it.key(), it.value()}, "pool_size",
                                        retval, &state::pool_size,
                                        &json::is_number, &json::get<int>))
          ;
        else if (get_conf<state>({it.key(), it.value()}, "command_file", retval,
                                 &state::command_file, &json::is_string))
          ;
        else if (get_conf<int, state>({it.key(), it.value()},
                                      "event_queue_max_size", retval,
                                      &state::event_queue_max_size,
                                      &json::is_number, &json::get<int>))
          ;
        else if (it.key() == "event_queues_total_size") {
          auto eqts = check_and_read<uint64_t>(json_document["centreonBroker"],
                                               "event_queues_total_size");
          retval.event_queues_total_size(eqts.value());
        } else if (it.key() == "output") {
          if (it.value().is_array()) {
            for (const json& node : it.value()) {
              try {
                endpoint out(endpoint::io_type::output);
                out.read_filters.insert("all");
                out.write_filters.insert("all");
                _parse_endpoint(node, out, module);
                retval.add_module(std::move(module));
                retval.add_endpoint(std::move(out));
              } catch (const deprecated& e) {
                logger_config->warn(
                    "Deprecated endpoint found in the output configuration: {}",
                    e.what());
              }
            }
          } else if (it.value().is_object()) {
            try {
              endpoint out(endpoint::io_type::output);
              out.read_filters.insert("all");
              out.write_filters.insert("all");
              _parse_endpoint(it.value(), out, module);
              retval.add_module(std::move(module));
              retval.add_endpoint(std::move(out));
            } catch (const deprecated& e) {
              logger_config->warn(
                  "Deprecated endpoint found in the output configuration: {}",
                  e.what());
            }
          } else
            throw msg_fmt(
                "config parser: cannot parse key '"
                "'output':  value type must be an object");
        } else if (it.key() == "input") {
          if (it.value().is_array()) {
            for (const json& node : it.value()) {
              try {
                endpoint in(endpoint::io_type::input);
                // in order to avoid event ping-pong of events
                // read filter allows all and write filter deny all
                in.read_filters.insert("all");
                _parse_endpoint(node, in, module);
                retval.add_module(std::move(module));
                retval.add_endpoint(std::move(in));
              } catch (const deprecated& e) {
                logger_config->warn(
                    "Deprecated endpoint found in the input configuration: {}",
                    e.what());
              }
            }
          } else if (it.value().is_object()) {
            try {
              endpoint in(endpoint::io_type::input);
              in.read_filters.insert("all");
              _parse_endpoint(it.value(), in, module);
              retval.add_module(std::move(module));
              retval.add_endpoint(std::move(in));
            } catch (const deprecated& e) {
              logger_config->warn(
                  "Deprecated endpoint found in the input configuration: {}",
                  e.what());
            }
          } else
            throw msg_fmt(
                "config parser: cannot parse key '"
                "'input':  value type must be an object");

        } else if (it.key() == "log") {
          if (!it.value().is_object())
            throw msg_fmt(
                "config parser: cannot parse key "
                "'log': value type must be an object");

          const json& conf_js = it.value();
          if (!conf_js.is_object())
            throw msg_fmt("the log configuration should be a json object");

          auto& conf = retval.mut_log_conf();
          if (conf_js.contains("directory") && conf_js["directory"].is_string())
            conf.set_dirname(conf_js["directory"].get<std::string>());
          else if (conf_js.contains("directory") &&
                   !conf_js["directory"].is_null())
            throw msg_fmt(
                "'directory' key in the log configuration must contain a "
                "directory name");
          if (conf.dirname().empty())
            conf.set_dirname("/var/log/centreon-broker");

          if (!misc::filesystem::writable(conf.dirname()))
            throw msg_fmt("The log directory '{}' is not writable",
                          conf.dirname());

          conf.set_filename("");

          if (conf_js.contains("filename") && conf_js["filename"].is_string()) {
            conf.set_filename(conf_js["filename"].get<std::string>());
            if (conf.filename().find("/") != std::string::npos)
              throw msg_fmt(
                  "'filename' must only contain a filename without directory");

          } else if (conf_js.contains("filename") &&
                     !conf_js["filename"].is_null())
            throw msg_fmt(
                "'filename' key in the log configuration must contain the log "
                "file name");

          auto ms = check_and_read<int64_t>(conf_js, "max_size");
          conf.set_max_size(ms ? ms.value() : 0u);

          auto fp = check_and_read<int64_t>(conf_js, "flush_period");
          if (fp) {
            if (fp.value() < 0)
              throw msg_fmt(
                  "'flush_period' key in the log configuration must contain a "
                  "positive number or 0.");

            conf.set_flush_interval(fp.value());
          } else
            conf.set_flush_interval(0u);

          auto lp = check_and_read<bool>(conf_js, "log_pid");
          conf.set_log_pid(lp ? lp.value() : false);

          auto ls = check_and_read<bool>(conf_js, "log_source");
          conf.set_log_source(ls ? ls.value() : false);

          if (conf_js.contains("loggers") && conf_js["loggers"].is_object()) {
            conf.loggers().clear();
            for (auto it = conf_js["loggers"].begin();
                 it != conf_js["loggers"].end(); ++it) {
              if (!log_v2::instance().contains_logger(it.key()))
                throw msg_fmt("'{}' is not available as logger", it.key());
              if (!it.value().is_string() || !log_v2::instance().contains_level(
                                                 it.value().get<std::string>()))
                throw msg_fmt(
                    "The logger '{}' must contain a string among 'trace', "
                    "'debug', 'info', 'warning', 'error', 'critical', "
                    "'disabled'",
                    it.key());

              conf.set_level(it.key(), it.value().get<std::string>());
            }
          }
        } else if (it.key() == "stats_exporter") {
          if (!it.value().is_object())
            throw msg_fmt(
                "config parser: cannot parse key 'stats_exporter': value type "
                "must be an object");
          if (it.value().contains("exporters")) {
            double export_interval;
            if (it.value().contains("export_interval")) {
              if (it.value()["export_interval"].is_number())
                export_interval = it.value()["export_interval"].get<double>();
              else
                throw msg_fmt(
                    "export_interval must be a number as it represents a "
                    "duration in seconds.");
            } else
              export_interval = 5;

            double export_timeout;
            if (it.value().contains("export_timeout")) {
              if (it.value()["export_timeout"].is_number())
                export_timeout = it.value()["export_timeout"].get<double>();
              else
                throw msg_fmt(
                    "export_timeout must be a number as it represents a "
                    "duration in seconds.");
            } else
              export_timeout = 0.5;

            retval.mut_stats_exporter().export_interval = export_interval;
            retval.mut_stats_exporter().export_timeout = export_timeout;
            for (auto& e : it.value()["exporters"]) {
              if (!e.is_object())
                throw msg_fmt(
                    "config parser: cannot parse content of 'stats_exporter': "
                    "values type must be an object");
              if (!e.contains("protocol"))
                throw msg_fmt(
                    "config parser: an exporter must contain a 'protocol' key "
                    "with 'http' or 'grpc' as value");
              if (!e.contains("url"))
                throw msg_fmt(
                    "config parser: an exporter must contain a 'url' key with "
                    "the server url as value");
              std::string proto_str = e["protocol"];
              state::stats_exporter::exporter_protocol protocol;
              if (absl::EqualsIgnoreCase(proto_str, "http"))
                protocol = state::stats_exporter::HTTP;
              else if (absl::EqualsIgnoreCase(proto_str, "grpc"))
                protocol = state::stats_exporter::GRPC;
              else
                throw msg_fmt(
                    "config parser: protocol values must be among \"http\" and "
                    "\"grpc\" in objects of stats_exporter");

              std::string url = e["url"];
              retval.mut_stats_exporter().exporters.emplace_back(
                  protocol, std::move(url));
            }
            retval.add_module("15-stats_exporter.so");
          } else
            logger_config->warn(
                "config parser: no exporters defined in the stats_exporter "
                "configuration");
        } else if (it.key() == "logger") {
          logger_config->warn("logger object is deprecated on 21.10");
        } else {
          if (it.key() == "stats")
            retval.add_module("15-stats.so");

          if (it.value().is_string())
            retval.params()[it.key()] = it.value().get<std::string>();
          else
            retval.params()[it.key()] = it.value().dump();
        }
      }
    }
  } catch (const json::parse_error& e) {
    throw msg_fmt("Config parser: Cannot parse the file '{}': {}", file,
                  e.what());
  }

  /* Post configuration */
  auto& conf = retval.mut_log_conf();
  if (conf.filename().empty())
    conf.set_filename(fmt::format("{}.log", retval.broker_name()));
  return retval;
}

void parser::_get_generic_endpoint_configuration(const json& elem,
                                                 endpoint& e) {
  e.cfg = elem;

  auto bt = check_and_read<time_t>(elem, "buffering_timeout");
  if (bt)
    e.buffering_timeout = bt.value();
  auto fo = check_and_read<std::string>(elem, "failover");
  if (fo)
    e.failovers.push_back(std::move(fo.value()));
  auto n = check_and_read<std::string>(elem, "name");
  if (n)
    e.name = std::move(n.value());
  auto rt = check_and_read<time_t>(elem, "read_timeout");
  if (rt)
    e.read_timeout = rt.value();
  auto ri = check_and_read<int32_t>(elem, "retry_interval");
  if (ri)
    e.retry_interval = ri.value();
}

/**
 *  Parse the configuration of an endpoint.
 *
 *  @param[in]  elem JSON element that has the endpoint configuration.
 *  @param[out] e    Element object.
 *  @param[out] module The module to load for this endpoint to work.
 */
void parser::_parse_endpoint(const json& elem,
                             endpoint& e,
                             std::string& module) {
  _get_generic_endpoint_configuration(elem, e);
  for (auto it = elem.begin(); it != elem.end(); ++it) {
    if (it.key() == "filters") {
      std::set<std::string> endpoint::*member;
      if (e.get_io_type() == endpoint::input)
        // if (e.write_filters.empty())  // Input.
        member = &endpoint::read_filters;
      else  // Output.
        member = &endpoint::write_filters;
      (e.*member).clear();
      if (it.value().is_object() && it.value()["category"].is_array())
        for (auto& cat : it.value()["category"])
          (e.*member).insert(cat.get<std::string>());
      else if (it.value().is_object() && it.value()["category"].is_string())
        (e.*member).insert(it.value()["category"].get<std::string>());
      else if (it.value().is_string() && it.value().get<std::string>() == "all")
        (e.*member).insert("all");
      else
        throw msg_fmt(
            "config parser: cannot parse key "
            "'filters':  value is invalid");
    } else if (it.key() == "cache") {
      auto cc = check_and_read<bool>(elem, "cache");
      e.cache_enabled = cc ? cc.value() : false;
    } else if (it.key() == "type") {
      e.type = it.value().get<std::string>();
      if (e.type == "ipv4" || e.type == "tcp" || e.type == "ipv6")
        module = "50-tcp.so";
      else if (e.type == "rrd")
        module = "70-rrd.so";
      else if (e.type == "sql")
        module = "80-sql.so";
      else if (e.type == "unified_sql")
        module = "20-unified_sql.so";
      else if (e.type == "storage")
        module = "20-storage.so";
      else if (e.type == "bam")
        module = "20-bam.so";
      else if (e.type == "bam_bi")
        module = "20-bam.so";
      else if (e.type == "lua")
        module = "70-lua.so";
      else if (e.type == "simu")
        module = "70-simu.so";
      else if (e.type == "graphite")
        module = "70-graphite.so";
      else if (e.type == "influxdb")
        module = "70-influxdb.so";
      else if (e.type == "victoria_metrics")
        module = "70-victoria_metrics.so";
      else if (e.type == "grpc")
        module = "50-grpc.so";
      else if (e.type == "bbdo_server" || e.type == "bbdo_client") {
        auto tp = check_and_read<std::string>(elem, "transport_protocol");
        if (tp) {
          if (absl::EqualsIgnoreCase(tp.value(), "tcp"))
            module = "50-tcp.so";
          else if (absl::EqualsIgnoreCase(tp.value(), "grpc"))
            module = "50-grpc.so";
          else
            throw msg_fmt(
                "config parser: 'transport_protocol' contains the value '{}' "
                "which is wrong ; it may contain only 'tcp' or 'grpc'",
                tp.value());
        } else
          throw msg_fmt(
              "config parser: A '{}' endpoint must have an entry "
              "'transport_protocol'",
              e.type);
      } else if (e.type == "file")
        throw deprecated(
            "'file' endpoint is deprecated and should not be used anymore");
      else
        throw msg_fmt("config parser: endpoint of invalid type '{}'", e.type);
    }
    if (it.value().is_string())
      e.params[it.key()] = it.value().get<std::string>();
    else
      log_v2::instance()
          .get(log_v2::CONFIG)
          ->debug(
              "config parser (while reading configuration file): "
              "for key: '{}' value is not a string.",
              it.key());
  }
}
