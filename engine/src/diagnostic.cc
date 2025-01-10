/**
 * Copyright 2013,2015 Merethis
 * Copyright 2020-2024 Centreon
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

#include "com/centreon/engine/diagnostic.hh"
#include <sys/stat.h>
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/version.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/process.hh"
#include "common/engine_conf/parser.hh"
#include "common/engine_conf/state_helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

/**
 *  Default constructor.
 */
diagnostic::diagnostic() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
diagnostic::diagnostic(diagnostic const& right) {
  (void)right;
}

/**
 *  Destructor.
 */
diagnostic::~diagnostic() {}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
diagnostic& diagnostic::operator=(diagnostic const& right) {
  (void)right;
  return *this;
}

/**
 *  Generate a diagnostic file.
 *
 *  @param[in] cfg_file Configuration file.
 *  @param[in] out_file Output file.
 */
void diagnostic::generate(std::string const& cfg_file,
                          std::string const& out_file) {
  // Destination directory.
  std::string tmp_dir;
  {
    char tmp_dir_ptr[] = "/tmp/brokerXXXXXX";
    if (!mkdtemp(tmp_dir_ptr))
      throw(engine_error()
            << "Cannot generate diagnostic temporary directory path.");
    tmp_dir = tmp_dir_ptr;
  }

  // Files to remove.
  std::vector<std::string> to_remove;

  // Base information about the software.
  std::cout << "Diagnostic: Centreon Engine " << CENTREON_ENGINE_VERSION_STRING
            << std::endl;

  // df.
  std::cout << "Diagnostic: Getting disk usage" << std::endl;
  {
    std::string df_log_path(tmp_dir + "/df.log");
    to_remove.push_back(df_log_path);
    _exec_and_write_to_file("df -P", df_log_path);
  }

  // lsb_release.
  std::cout << "Diagnostic: Getting LSB information" << std::endl;
  {
    std::string lsb_release_log_path(tmp_dir + "/lsb_release.log");
    to_remove.push_back(lsb_release_log_path);
    _exec_and_write_to_file("lsb_release -a", lsb_release_log_path);
  }

  // uname.
  std::cout << "Diagnostic: Getting system name" << std::endl;
  {
    std::string uname_log_path(tmp_dir + "/uname.log");
    to_remove.push_back(uname_log_path);
    _exec_and_write_to_file("uname -a", uname_log_path);
  }

  // /proc/version
  std::cout << "Diagnostic: Getting kernel information" << std::endl;
  {
    std::string proc_version_log_path(tmp_dir + "/proc_version.log");
    to_remove.push_back(proc_version_log_path);
    _exec_and_write_to_file("cat /proc/version", proc_version_log_path);
  }

  // netstat.
  std::cout << "Diagnostic: Getting network connections information"
            << std::endl;
  {
    std::string netstat_log_path(tmp_dir + "/netstat.log");
    to_remove.push_back(netstat_log_path);
    _exec_and_write_to_file("netstat -ap --numeric-hosts", netstat_log_path);
  }

  // ps.
  std::cout << "Diagnostic: Getting processes information" << std::endl;
  {
    std::string ps_log_path(tmp_dir + "/ps.log");
    to_remove.push_back(ps_log_path);
    _exec_and_write_to_file("ps aux", ps_log_path);
  }

  // rpm.
  std::cout << "Diagnostic: Getting packages information" << std::endl;
  {
    std::string rpm_log_path(tmp_dir + "/rpm.log");
    to_remove.push_back(rpm_log_path);
    _exec_and_write_to_file("rpm -qa centreon*", rpm_log_path);
  }

  // sestatus.
  std::cout << "Diagnostic: Getting SELinux status" << std::endl;
  {
    std::string sestatus_log_path(tmp_dir + "/selinux.log");
    to_remove.push_back(sestatus_log_path);
    _exec_and_write_to_file("sestatus", sestatus_log_path);
  }

  // Parse configuration file.
  std::cout << "Diagnostic: Parsing configuration file '" << cfg_file << "'"
            << std::endl;
  configuration::State conf;
  try {
    configuration::error_cnt err;
    configuration::parser parsr;
    parsr.parse(cfg_file, &conf, err);
  } catch (std::exception const& e) {
    std::cerr << "Diagnostic: configuration file '" << cfg_file
              << "' parsing failed: " << e.what() << std::endl;
  }

  // Create temporary configuration directory.
  std::string tmp_cfg_dir(tmp_dir + "/cfg/");
  if (mkdir(tmp_cfg_dir.c_str(), S_IRWXU)) {
    char const* msg(strerror(errno));
    throw(engine_error() << "Cannot create temporary configuration directory '"
                         << tmp_cfg_dir << "': " << msg);
  }

  // Copy base configuration file.
  std::cout << "Diagnostic: Copying configuration files" << std::endl;
  {
    std::string target_path(_build_target_path(tmp_cfg_dir, cfg_file));
    to_remove.push_back(target_path);
    _exec_cp(cfg_file, target_path);
  }

  // Copy other configuration files.
  for (auto it = conf.cfg_file().begin(), end = conf.cfg_file().end();
       it != end; ++it) {
    std::string target_path(_build_target_path(tmp_cfg_dir, *it));
    to_remove.push_back(target_path);
    _exec_cp(*it, target_path);
  }

  // Create temporary log directory.
  std::string tmp_log_dir(tmp_dir + "/log/");
  if (mkdir(tmp_log_dir.c_str(), S_IRWXU)) {
    char const* msg(strerror(errno));
    throw(engine_error() << "Cannot create temporary log directory '"
                         << tmp_log_dir << "': " << msg);
  }

  // Log file.
  std::cout << "Diagnostic: getting log file" << std::endl;
  {
    std::string target_path(_build_target_path(tmp_log_dir, conf.log_file()));
    to_remove.push_back(target_path);
    _exec_cp(conf.log_file(), target_path);
  }

  // Debug file.
  std::cout << "Diagnostic: getting debug file" << std::endl;
  {
    std::string target_path(_build_target_path(tmp_log_dir, conf.debug_file()));
    to_remove.push_back(target_path);
    _exec_cp(conf.debug_file(), target_path);
  }

  // Retention file.
  std::cout << "Diagnostic: getting retention file" << std::endl;
  {
    std::string target_path(
        _build_target_path(tmp_log_dir, conf.state_retention_file()));
    to_remove.push_back(target_path);
    _exec_cp(conf.state_retention_file(), target_path);
  }

  // Status file.
  std::cout << "Diagnostic: getting status file" << std::endl;
  {
    std::string target_path(
        _build_target_path(tmp_log_dir, conf.status_file()));
    to_remove.push_back(target_path);
    _exec_cp(conf.status_file(), target_path);
  }

  // Generate file name if not existing.
  std::string my_out_file;
  if (out_file.empty())
    my_out_file = "centengine-diag.tar.gz";
  else
    my_out_file = out_file;

  // Create tarball.
  std::cout << "Diagnostic: Creating tarball '" << my_out_file << "'"
            << std::endl;
  {
    std::ostringstream cmdline;
    cmdline << "tar czf '" << my_out_file << "' '" << tmp_dir << "'";
    process p;
    p.exec(cmdline.str());
    p.wait();
  }

  // Cleanup.
  for (std::vector<std::string>::const_iterator it(to_remove.begin()),
       end(to_remove.end());
       it != end; ++it)
    ::remove(it->c_str());
  rmdir(tmp_log_dir.c_str());
  rmdir(tmp_cfg_dir.c_str());
  rmdir(tmp_dir.c_str());
}

/**
 *  Create path base on the temporary base path and the current file
 *  path.
 *
 *  @param[in] base The base path.
 *  @param[in] file The current file path.
 *
 *  @return The new path.
 */
std::string diagnostic::_build_target_path(std::string const& base,
                                           std::string const& file) {
  std::string target_path(base);
  size_t pos(file.find_last_of('/'));
  if (pos != std::string::npos)
    target_path.append(file.substr(pos + 1));
  else
    target_path.append(file);
  return target_path;
}

/**
 *  Execute a command and write its result to a file.
 *
 *  @param[in] cmd      Command file.
 *  @param[in] out_file Output file.
 */
void diagnostic::_exec_and_write_to_file(std::string const& cmd,
                                         std::string const& out_file) {
  std::string result;
  {
    process p;
    p.exec(cmd);
    p.wait();
    p.read(result);
  }
  io::file_stream fs;
  fs.open(out_file, "w");
  while (!result.empty()) {
    unsigned long wb(fs.write(result.data(), result.size()));
    result.erase(0, wb);
  }
}

/**
 *  Execute cp command.
 *
 *  @param[in] src The source file to copy.
 *  @param[in] dst The destination file to copy.
 */
void diagnostic::_exec_cp(std::string const& src, std::string const& dst) {
  std::ostringstream oss;
  oss << "cp '" << src << "' '" << dst << "'";
  process p;
  p.exec(oss.str());
  p.wait();
}
