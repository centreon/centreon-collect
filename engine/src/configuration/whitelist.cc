/*
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

#include <fnmatch.h>
#include <grp.h>
#include <sys/types.h>

#include <experimental/filesystem>
#include <iostream>

#include "absl/base/call_once.h"

#include <boost/exception/all.hpp>
#include <ryml.hpp>

#include "com/centreon/engine/configuration/whitelist.hh"
#include "com/centreon/engine/log_v2.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

CCE_BEGIN()

namespace configuration {

const std::string command_blacklist_output(
    "UNKNOWN: this command cannot be executed because of security restrictions "
    "on the poller. A whitelist has been defined, and it does not include this "
    "command.");

/**
 * @brief rapidyaml call abort on error
 * the goal of these structs and local function is to throw a
 * rapid_yaml_exception instead
 *
 */
using err_info_rapidyaml =
    boost::error_info<struct err_info_rapidyaml_, std::string>;

struct rapid_yaml_exception : public virtual boost::exception,
                              public virtual std::exception {};

// by default on error rapidyaml call abort so this handler
void on_rapidyaml_error(const char* buff,
                        size_t length,
                        ryml::Location loc,
                        void*) {
  rapid_yaml_exception ex;
  ex << boost::throw_file(loc.name.data()) << boost::throw_line(loc.line);
  ex << err_info_rapidyaml(std::string(buff, length));
  throw ex;
}

}  // namespace configuration

CCE_END();

/**
 * @brief before using rapidyaml, we change error handler
 *
 */
void whitelist_file::init_ryml_error_handler() {
  static absl::once_flag _initialized;
  absl::call_once(_initialized, []() {
    ryml::set_callbacks(
        ryml::Callbacks(nullptr, nullptr, nullptr, on_rapidyaml_error));
  });
}

/**
 * @brief read and parse a json or yaml file
 *
 */
void whitelist_file::parse() {
  size_t file_size = 0;
  try {
    // check file
    struct stat infos;
    if (::stat(_path.c_str(), &infos)) {
      SPDLOG_LOGGER_ERROR(log_v2::config(), "{} doesn't exist", _path);
      BOOST_THROW_EXCEPTION(open_file_exception()
                            << boost::errinfo_file_name(_path));
    }
    if ((infos.st_mode & S_IFMT) != S_IFREG) {
      SPDLOG_LOGGER_ERROR(log_v2::config(), "{} is not a regular file", _path);
      BOOST_THROW_EXCEPTION(open_file_exception()
                            << boost::errinfo_file_name(_path));
    }
    _last_file_write = infos.st_mtime;
    if (!infos.st_size) {
      SPDLOG_LOGGER_ERROR(log_v2::config(), "{} is an empty file", _path);
      BOOST_THROW_EXCEPTION(open_file_exception()
                            << boost::errinfo_file_name(_path));
    }

    struct ::group* centengine_group = getgrnam("centreon-engine");

    if (centengine_group) {
      if (infos.st_uid || infos.st_gid != centengine_group->gr_gid) {
        SPDLOG_LOGGER_ERROR(log_v2::config(),
                            "file {} must be owned by root@centreon-engine",
                            _path);
      }
    }
    if (infos.st_mode & S_IRWXO || (infos.st_mode & S_IRWXG) != S_IRGRP) {
      SPDLOG_LOGGER_ERROR(log_v2::config(),
                          "file {} must have x40 right access", _path);
    }

    file_size = infos.st_size;
  } catch (boost::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(), "fail to open {}: {}", _path,
                        boost::diagnostic_information(e));
    throw;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(), "fail to open {}: {}", _path,
                        boost::diagnostic_information(e));
    throw;
  }
  // read file
  std::unique_ptr<char[]> buff(new char[file_size]);
  try {
    std::ifstream f(_path);
    size_t read = 0;
    while (read != file_size) {
      std::streamsize some_read =
          f.readsome(buff.get() + read, file_size - read);
      if (some_read < 0) {
        BOOST_THROW_EXCEPTION(open_file_exception()
                              << boost::errinfo_file_name(_path));
      }
      read += some_read;
    }
  } catch (const boost::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(), "fail to read {}: {}", _path,
                        boost::diagnostic_information(e));
    throw;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(), "fail to read {}: {}", _path,
                        boost::diagnostic_information(e));
    throw;
  }
  // init rapidyaml error handler
  init_ryml_error_handler();
  // parse file content
  try {
    // parse in place more efficient so we copy read only mapping
    ryml::Tree tree = ryml::parse_in_place(ryml::substr(buff.get(), file_size));
    _read_file_content(tree);
  } catch (const boost::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(), "fail to parse {}: {}", _path,
                        boost::diagnostic_information(e));
    throw;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(), "fail to parse {}: {}", _path,
                        boost::diagnostic_information(e));
    throw;
  }
}

/**
 * @brief file content has been decoded in an ryml::Tree, now we extract data
 *
 * @tparam ryml_tree ryml::Tree
 * @param file_content
 */
template <class ryml_tree>
void whitelist_file::_read_file_content(const ryml_tree& file_content) {
  ryml::ConstNodeRef root = file_content["whitelist"];
  ryml::ConstNodeRef wildcards = root.find_child("wildcard");
  ryml::ConstNodeRef regexps = root.find_child("regex");
  if (wildcards.valid() && !wildcards.empty()) {
    if (!wildcards.is_seq()) {  // not an array => throw
      SPDLOG_LOGGER_ERROR(log_v2::config(), "{}: wildcards is not a sequence");
      BOOST_THROW_EXCEPTION(yaml_structure_exception() << err_info_rapidyaml(
                                "wildcards is not a sequence"));
    }
    for (auto wildcard : wildcards) {
      auto value = wildcard.val();
      std::string_view str_value(value.data(), value.size());
      SPDLOG_LOGGER_INFO(log_v2::config(),
                         "{} wildcard '{}' added to whitelist", _path,
                         str_value);
      _wildcards.emplace_back(str_value);
    }
  }
  if (regexps.valid() && !regexps.empty()) {
    if (!regexps.is_seq()) {  // not an array => throw
      SPDLOG_LOGGER_ERROR(log_v2::config(), "{}: regexps is not a sequence");
      BOOST_THROW_EXCEPTION(yaml_structure_exception()
                            << err_info_rapidyaml("regexps is not a sequence"));
    }
    for (auto re : regexps) {
      auto value = re.val();
      std::string_view str_value(value.data(), value.size());
      std::unique_ptr<re2::RE2> to_push_back =
          std::make_unique<re2::RE2>(str_value);
      if (to_push_back->error_code() ==
          re2::RE2::ErrorCode::NoError) {  // success compile regex
        SPDLOG_LOGGER_INFO(log_v2::config(),
                           "{} regexp '{}' added to whitelist", _path,
                           str_value);
        _regex.push_back(std::move(to_push_back));
      } else {  // bad regex
        SPDLOG_LOGGER_ERROR(
            log_v2::config(), "fail to parse regex {}: error: {} at {} ",
            str_value, to_push_back->error(), to_push_back->error_arg());
      }
    }
  }
}

/**
 * @brief test if a cmdline matches
 *
 * @param cmdline
 * @return true  cmdline matches to at least one regex or wildcard
 * @return false  cmdline don't match
 */
bool whitelist_file::test(const std::string& cmdline) const {
  // remove double /
  if (cmdline.find("//") != std::string::npos) {
    std::string copy = boost::algorithm::replace_all_copy(cmdline, "//", "/");
    for (const std::string& wildcard : _wildcards) {
      if (!fnmatch(wildcard.c_str(), copy.c_str(), FNM_PATHNAME | FNM_PERIOD))
        return true;
    }
  } else {
    for (const std::string& wildcard : _wildcards) {
      if (!fnmatch(wildcard.c_str(), cmdline.c_str(),
                   FNM_PATHNAME | FNM_PERIOD))
        return true;
    }
  }

  for (const auto& regex : _regex) {
    if (RE2::FullMatch(cmdline, *regex))
      return true;
  }
  return false;
}

/**
 * @brief create a whitelist_file
 *
 * @tparam str
 * @param path  file path
 * @return std::unique_ptr<whitelist_file>  null if parse throw
 */
template <typename str>
std::unique_ptr<whitelist_file> whitelist_file::create(const str& path) {
  try {
    std::unique_ptr<whitelist_file> ret =
        std::make_unique<whitelist_file>(path);
    ret->parse();
    return ret;
  } catch (const std::exception&) {
    return std::unique_ptr<whitelist_file>();
  }
}

/**
 * @brief scan whitelist directory and refresh whitelist_file list
 *
 */
void whitelist_directory::refresh() {
  // check permissions
  struct stat dir_infos;
  if (::stat(_path.c_str(), &dir_infos)) {
    SPDLOG_LOGGER_INFO(
        log_v2::config(),
        "{}: no whitelist directory found, all commands are accepted", _path);
    return;
  }
  if ((dir_infos.st_mode & S_IFMT) != S_IFDIR) {
    SPDLOG_LOGGER_ERROR(log_v2::config(), "{} is not a directory: {}", _path,
                        dir_infos.st_mode);
    return;
  }

  struct ::group* centengine_group = getgrnam("centreon-engine");

  if (centengine_group) {
    if (dir_infos.st_uid || dir_infos.st_gid != centengine_group->gr_gid) {
      SPDLOG_LOGGER_ERROR(log_v2::config(),
                          "directory {} must be owned by root@centreon-engine",
                          _path);
    }
  }

  if (dir_infos.st_mode & S_IRWXO ||
      (dir_infos.st_mode & S_IRWXG) != S_IRGRP + S_IXGRP) {
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "directory {} must have 750 right access", _path);
  }

  // all must be sorted in order to perform an incremental comparaison
  std::set<std::string> files_in_directory;
  for (const auto& dir_entry :
       std::experimental::filesystem::directory_iterator{_path}) {
    if (dir_entry.status().type() ==
        std::experimental::filesystem::file_type::regular) {
      files_in_directory.insert(dir_entry.path().generic_string());
    }
  }

  if (files_in_directory.empty()) {
    _files.clear();
    SPDLOG_LOGGER_INFO(log_v2::config(),
                       "{}: whitelist directory found, but no restrictions, "
                       "all commands are accepted",
                       _path);
    return;
  }

  std::set<std::string>::const_iterator child_iter = files_in_directory.begin();
  std::set<std::string>::const_iterator child_end = files_in_directory.end();

  std::vector<std::unique_ptr<whitelist_file>>::iterator whitelist_iter =
      _files.begin();

  while (child_iter != child_end && whitelist_iter != _files.end()) {
    int cmp = child_iter->compare((*whitelist_iter)->get_path());
    if (cmp < 0) {  // new file
      std::unique_ptr<whitelist_file> to_add =
          whitelist_file::create(*child_iter);
      if (to_add && !to_add->empty()) {  // file correct
        whitelist_iter = _files.emplace(whitelist_iter, std::move(to_add));
        ++whitelist_iter;
      }
      ++child_iter;
    } else if (cmp > 0) {  // deleted file
      whitelist_iter = _files.erase(whitelist_iter);
    } else {  // file has changed?
      struct stat file_infos;
      ::stat(child_iter->c_str(), &file_infos);
      if (file_infos.st_mtime != (*whitelist_iter)->get_last_file_write()) {
        std::unique_ptr<whitelist_file> update =
            whitelist_file::create(*child_iter);
        if (update && !update->empty()) {  // file correct
          *whitelist_iter = std::move(update);
          ++whitelist_iter;
        } else
          whitelist_iter = _files.erase(whitelist_iter);
      } else {
        ++whitelist_iter;
      }
      ++child_iter;
    }
  }
  // some files to delete?
  while (whitelist_iter != _files.end()) {
    whitelist_iter = _files.erase(whitelist_iter);
  }
  // some new files
  for (; child_iter != child_end; ++child_iter) {
    std::unique_ptr<whitelist_file> to_add =
        whitelist_file::create(*child_iter);
    if (to_add)
      _files.emplace_back(std::move(to_add));
  }
}

/**
 * @brief test cmdline with each whitelist file
 * beware, if file list is empty, return always true
 *
 * @param cmdline
 * @return true match to at least one regex or wildcard
 * @return false
 */
bool whitelist_directory::test(const std::string& cmdline) const {
  if (_files.empty()) {
    return true;
  }
  for (const auto& file : _files) {
    if (file->test(cmdline)) {
      return true;
    }
  }
  SPDLOG_LOGGER_INFO(log_v2::checks(), "command rejected by whitelist: {}",
                     cmdline);
  return false;
}

/**
 * @brief test cmdline with each whitelist file
 *
 * @param cmdline
 * @return true match to at least one regex or wildcard
 * @return false
 */
bool whitelist_directories::test(const std::string& cmdline) const {
  if (_directories.empty()) {
    return true;
  }
  for (const whitelist_directory& dir : _directories) {
    if (dir.test(cmdline)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief scan whitelist directories and refresh whitelist_file list
 *
 */
void whitelist_directories::refresh() {
  for (whitelist_directory& dir : _directories) {
    dir.refresh();
  }
}