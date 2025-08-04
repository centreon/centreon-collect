/**
 * Copyright 2023 Centreon (https://www.centreon.com/)
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
#define C4_NO_DEBUG_BREAK 1

#include "com/centreon/engine/configuration/whitelist.hh"

#include <fnmatch.h>
#include <grp.h>
#include <sys/types.h>

#include <filesystem>
#include <iostream>
#include <ryml/ryml.hpp>

#include "absl/base/call_once.h"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon;
using namespace com::centreon::exceptions;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using com::centreon::common::log_v2::log_v2;

namespace com::centreon::engine::configuration {

const std::string command_blacklist_output(
    "UNKNOWN: this command cannot be executed because of security restrictions "
    "on the poller. A whitelist has been defined, and it does not include this "
    "command.");

/**
 * @brief rapidyaml call abort on error
 * the goal of these structs and local function is to throw a
 * exception instead
 * by default on error rapidyaml call abort so this handler
 */
void on_rapidyaml_error(const char* buff,
                        size_t length [[maybe_unused]],
                        ryml::Location loc,
                        void*) {
  throw msg_fmt("fail to parse {} at line {}: {}", loc.name.data(), loc.line,
                buff);
}

}  // namespace com::centreon::engine::configuration

std::unique_ptr<whitelist> whitelist::_instance;

/**
 * @brief before using rapidyaml, we change error handler
 *
 */
void whitelist::init_ryml_error_handler() {
  static absl::once_flag _initialized;
  absl::call_once(_initialized, []() {
    ryml::set_callbacks(
        ryml::Callbacks(nullptr, nullptr, nullptr, on_rapidyaml_error));
  });
}

static std::atomic_uint _instance_gen(1);

/**
 * @brief Construct a new whitelist::whitelist object
 * used only by UT tests
 *
 * @param file_path
 */
whitelist::whitelist(const std::string_view& file_path)
    : _logger{log_v2::instance().get(log_v2::CONFIG)},
      _instance_id(_instance_gen.fetch_add(1)) {
  init_ryml_error_handler();
  _parse_file(file_path);
}

/**
 * @brief read and parse a json or yaml file
 * @return true  if file contains at least one regex or wildcard
 *
 */
bool whitelist::_parse_file(const std::string_view& file_path) {
  // check file
  struct stat infos;
  if (::stat(file_path.data(), &infos)) {
    SPDLOG_LOGGER_ERROR(_logger, "{} doesn't exist", file_path);
    return false;
  }
  if ((infos.st_mode & S_IFMT) != S_IFREG) {
    SPDLOG_LOGGER_ERROR(_logger, "{} is not a regular file", file_path);
    return false;
  }
  if (!infos.st_size) {
    SPDLOG_LOGGER_ERROR(_logger, "{} is an empty file", file_path);
    return false;
  }

  struct ::group* centengine_group = getgrnam("centreon-engine");

  if (centengine_group) {
    if (infos.st_uid || infos.st_gid != centengine_group->gr_gid) {
      SPDLOG_LOGGER_ERROR(
          _logger, "file {} must be owned by root@centreon-engine", file_path);
    }
  }
  if (infos.st_mode & S_IRWXO || (infos.st_mode & S_IRWXG) != S_IRGRP) {
    SPDLOG_LOGGER_ERROR(_logger, "file {} must have x40 right access",
                        file_path);
  }

  size_t file_size = infos.st_size;
  // read file
  std::unique_ptr<char[]> buff(new char[file_size]);
  try {
    std::ifstream f(file_path.data());
    size_t read = 0;
    while (read != file_size) {
      std::streamsize some_read =
          f.readsome(buff.get() + read, file_size - read);
      if (some_read <= 0) {
        SPDLOG_LOGGER_ERROR(_logger, "fail to read {}", file_path);
        return false;
      }
      read += some_read;
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to read {}: {}", file_path, e.what());
    return false;
  }
  // parse file content
  try {
    // parse in place more efficient so we copy read only mapping
    ryml::Tree tree = ryml::parse_in_place(ryml::substr(buff.get(), file_size));
    return _read_file_content(tree);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to parse {}: {}", file_path, e.what());
    return false;
  }
}

/**
 * @brief file content has been decoded in an ryml::Tree, now we extract data
 *
 * @tparam ryml_tree ryml::Tree
 * @param file_content
 */
template <class ryml_tree>
bool whitelist::_read_file_content(const ryml_tree& file_content) {
  ryml::ConstNodeRef root = file_content;

  bool ret = false;

  auto load_ruleset = [&](const ryml::ConstNodeRef& node, rule_set& rules) {
    // add whildcard and regexps
    if (node.has_child("wildcard")) {
      auto wc_node = node["wildcard"];
      if (wc_node.is_seq()) {
        for (auto wildcard : wc_node) {
          auto value = wildcard.val();
          if (value.size() > 0) {
            std::string_view str_value(value.data(), value.size());
            SPDLOG_LOGGER_INFO(_logger, "wildcard '{}' added to whitelist",
                               str_value);
            rules.wildcard.emplace_back(str_value);
            ret = true;
          }
        }
      } else {
        SPDLOG_LOGGER_ERROR(_logger, "wildcard is not a sequence");
      }
    }

    if (node.has_child("regex")) {
      auto re_node = node["regex"];
      if (re_node.is_seq()) {
        for (auto re : re_node) {
          auto value = re.val();
          if (value.size() > 0) {
            std::string_view str_value(value.data(), value.size());
            std::unique_ptr<re2::RE2> to_push_back =
                std::make_unique<re2::RE2>(str_value);
            if (to_push_back->error_code() == re2::RE2::ErrorCode::NoError) {
              SPDLOG_LOGGER_INFO(_logger, "regexp '{}' added to whitelist",
                                 str_value);
              rules.regex.push_back(std::move(to_push_back));
              ret = true;
            } else {
              SPDLOG_LOGGER_ERROR(_logger, "fail to parse {}: error: {} at {} ",
                                  str_value, to_push_back->error(),
                                  to_push_back->error_arg());
            }
          }
        }
      } else {
        SPDLOG_LOGGER_ERROR(_logger, "regex is not a sequence");
      }
    }
  };

  /* 1. whitelist -----------------------------------------------------------*/
  if (root.has_child("whitelist")) {
    ryml::ConstNodeRef whitelist = root["whitelist"];
    load_ruleset(whitelist, _config.engine);
  }

  /* 2. cma-whitelist (optional) ------------------------------------------- */
  if (root.has_child("cma-whitelist")) {
    ryml::ConstNodeRef cma = root["cma-whitelist"];

    /* default ----------------------------------------------------------- */
    if (cma.has_child("default")) {
      auto seq = cma["default"];
      load_ruleset(seq, _config.cma.defaults);
    }

    /* hosts ------------------------------------------------------------- */
    if (cma.has_child("hosts")) {
      ryml::ConstNodeRef hosts = cma["hosts"];
      for (const ryml::ConstNodeRef& host : hosts) {
        auto hostname = host["hostname"].val();
        if (hostname.size() > 0) {
          std::string_view str_hostname(hostname.data(), hostname.size());
          SPDLOG_LOGGER_INFO(_logger, "cma whitelist for host '{}'",
                             str_hostname);
          rule_set& rules = _config.cma.hosts[std::string(str_hostname)];
          load_ruleset(host, rules);
          ret = true;
        } else {
          SPDLOG_LOGGER_ERROR(_logger, "cma whitelist host name is empty");
        }
      }
    }
  }
  return ret;
}

/**
 * @brief Determines if a command is permitted according to configured wildcard
 * and regex rules.
 *
 * The cmdline is first normalized by collapsing any "//" sequences into a
 * single "/". If the provided rule_set contains no wildcard or regex patterns,
 * the command is always allowed. Otherwise, this function:
 *   1. Applies each wildcard pattern (fnmatch with FNM_PATHNAME | FNM_PERIOD).
 *   2. If no wildcard matches, applies each regex pattern (RE2::FullMatch).
 * The command is allowed as soon as one pattern matches.
 *
 * @param cmdline Full command line (macros already expanded) to test.
 * @param rules   Rule set containing wildcard and regex patterns.
 * @return true  cmdline matches at least one wildcard or regex rule.
 * @return false cmdline does not match any configured rule.
 */
bool whitelist::_is_allowed(const std::string& cmdline, const rule_set& rules) {
  if (rules.wildcard.empty() && rules.regex.empty()) {
    return true;
  }
  auto check_cmd_line = [&](const std::string& clean_cmdline) -> bool {
    for (const std::string& wildcard : rules.wildcard) {
      if (!fnmatch(wildcard.c_str(), clean_cmdline.c_str(),
                   FNM_PATHNAME | FNM_PERIOD)) {
        return true;
      }
    }

    for (const auto& regex : rules.regex) {
      if (RE2::FullMatch(clean_cmdline, *regex)) {
        return true;
      }
    }
    return false;
  };

  // remove double /
  if (cmdline.find("//") != std::string::npos) {
    return check_cmd_line(
        boost::algorithm::replace_all_copy(cmdline, "//", "/"));
  } else {
    return check_cmd_line(cmdline);
  }
}

/**
 * Check if a command is allowed by the CMA whitelist.
 * Uses host-specific rules when available, otherwise falls back to defaults.
 *
 * @param cmdline Final command line (macros expanded)
 * @param hostname Host name to check (empty = use default rules)
 * @return true if the command is allowed, false otherwise
 */
bool whitelist::is_allowed_by_cma(const std::string& cmdline,
                                  const std::string& hostname) {
  const auto& defaults = _config.cma.defaults;
  if (defaults.wildcard.empty() && defaults.regex.empty() &&
      _config.cma.hosts.empty())
    return true;

  if (!hostname.empty()) {
    auto it = _config.cma.hosts.find(hostname);
    if (it != _config.cma.hosts.end())
      return _is_allowed(cmdline, it->second);
  }
  return _is_allowed(cmdline, defaults);
}

/**
 * @brief parse all whitelist files in a directory
 *
 * @param directory
 * @return whitelist::e_refresh_result
 */
whitelist::e_refresh_result whitelist::parse_dir(
    const std::string_view directory) {
  // check permissions
  struct stat dir_infos;
  if (::stat(directory.data(), &dir_infos)) {
    return e_refresh_result::no_directory;
  }
  if ((dir_infos.st_mode & S_IFMT) != S_IFDIR) {
    SPDLOG_LOGGER_ERROR(_logger, "{} is not a directory: {}", directory,
                        dir_infos.st_mode);
    return e_refresh_result::no_directory;
  }

  struct ::group* centengine_group = getgrnam("centreon-engine");

  if (centengine_group) {
    if (dir_infos.st_uid || dir_infos.st_gid != centengine_group->gr_gid) {
      SPDLOG_LOGGER_ERROR(_logger,
                          "directory {} must be owned by root@centreon-engine",
                          directory);
    }
  }

  if (dir_infos.st_mode & S_IRWXO ||
      (dir_infos.st_mode & S_IRWXG) != S_IRGRP + S_IXGRP) {
    SPDLOG_LOGGER_ERROR(_logger, "directory {} must have 750 right access",
                        directory);
  }

  try {
    e_refresh_result res = e_refresh_result::empty_directory;
    // all must be sorted in order to perform an incremental comparaison
    for (const auto& dir_entry :
         std::filesystem::directory_iterator{directory}) {
      if (dir_entry.status().type() == std::filesystem::file_type::regular) {
        if (res < e_refresh_result::no_rule) {
          res = e_refresh_result::no_rule;
        }
        if (_parse_file(dir_entry.path().generic_string())) {
          res = e_refresh_result::rules;
        }
      }
    }
    return res;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to read {} directory: {}", directory,
                        e.what());
    return e_refresh_result::no_directory;
  }
}

whitelist& whitelist::instance() {
  if (!_instance) {
    reload();
  }
  return *_instance;
}

void whitelist::reload() {
  static constexpr std::string_view directories[] = {
      "/etc/centreon-engine-whitelist",
      "/usr/share/centreon-engine/whitelist.conf.d"};
  _instance = std::make_unique<whitelist>(directories, directories + 2);
}
