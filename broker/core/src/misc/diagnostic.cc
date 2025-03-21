/**
 * Copyright 2013,2015 Centreon
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

#include "com/centreon/broker/misc/diagnostic.hh"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::misc;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Default constructor.
 */
diagnostic::diagnostic() : _logger{log_v2::instance().get(log_v2::CORE)} {}

/**
 *  Generate diagnostic file.
 *
 *  @param[in]  cfg_files Main configuration files.
 *  @param[out] out_file  Output file.
 */
void diagnostic::generate(std::vector<std::string> const& cfg_files,
                          std::string const& out_file) {
  // Destination directory.
  std::string tmp_dir{temp_path()};
  struct stat st;
  if (stat(tmp_dir.c_str(), &st) == -1) {
    mkdir(tmp_dir.c_str(), 0700);
  }

  // Files to remove.
  std::list<std::string> to_remove;

  // Add diagnostic log file.
  config::state diagnostic_state;

  std::string diagnostic_log_path{fmt::format("{}/diagnostic.log", tmp_dir)};
  to_remove.push_back(diagnostic_log_path);

  // Base information about the software.
  _logger->info("diagnostic: Centreon Broker {}", CENTREON_BROKER_VERSION);

  // df.
  _logger->info("diagnostic: getting disk usage");
  {
    std::string df_log_path;
    df_log_path = tmp_dir;
    df_log_path.append("/df.log");
    to_remove.push_back(df_log_path);
    std::string output{misc::exec("df -P")};

    std::ofstream out(df_log_path);
    out << output;
    out.close();
  }

  // lsb_release.
  _logger->info("diagnostic: getting LSB information");
  {
    std::string lsb_release_log_path;
    lsb_release_log_path = tmp_dir;
    lsb_release_log_path.append("/lsb_release.log");
    to_remove.push_back(lsb_release_log_path);
    std::string output{misc::exec("lsb_release -a")};

    std::ofstream out(lsb_release_log_path);
    out << output;
    out.close();
  }

  // uname.
  _logger->info("diagnostic: getting system name");
  {
    std::string uname_log_path;
    uname_log_path = tmp_dir;
    uname_log_path.append("/uname.log");
    to_remove.push_back(uname_log_path);
    std::string output{misc::exec("uname -a")};

    std::ofstream out(uname_log_path);
    out << output;
    out.close();
  }

  // /proc/version
  _logger->info("diagnostic: getting kernel information");
  {
    std::string proc_version_log_path;
    proc_version_log_path = tmp_dir;
    proc_version_log_path.append("/proc_version.log");
    to_remove.push_back(proc_version_log_path);
    std::string output{misc::exec("cat /proc/version")};

    std::ofstream out(proc_version_log_path);
    out << output;
    out.close();
  }

  // netstat.
  _logger->info("diagnostic: getting network connections information");
  {
    std::string netstat_log_path;
    netstat_log_path = tmp_dir;
    netstat_log_path.append("/netstat.log");
    to_remove.push_back(netstat_log_path);
    std::string output{misc::exec("netstat -ap --numeric-hosts")};

    std::ofstream out(netstat_log_path);
    out << output;
    out.close();
  }

  // ps.
  _logger->info("diagnostic: getting processes information");
  {
    std::string ps_log_path;
    ps_log_path = tmp_dir;
    ps_log_path.append("/ps.log");
    to_remove.push_back(ps_log_path);
    std::string output{misc::exec("ps aux")};

    std::ofstream out(ps_log_path);
    out << output;
    out.close();
  }

  // rpm.
  _logger->info("diagnostic: getting packages information");
  {
    std::string rpm_log_path;
    rpm_log_path = tmp_dir;
    rpm_log_path.append("/rpm.log");
    to_remove.push_back(rpm_log_path);
    std::string output{misc::exec("rpm -qa centreon")};

    std::ofstream out(rpm_log_path);
    out << output;
    out.close();
  }

  // sestatus.
  _logger->info("diagnostic: getting SELinux status");
  {
    std::string selinux_log_path;
    selinux_log_path = tmp_dir;
    selinux_log_path.append("/selinux.log");
    to_remove.push_back(selinux_log_path);
    std::string output{misc::exec("sestatus")};

    std::ofstream out(selinux_log_path);
    out << output;
    out.close();
  }

  // Browse configuration files.
  for (std::vector<std::string>::const_iterator it(cfg_files.begin()),
       end(cfg_files.end());
       it != end; ++it) {
    // Configuration file.
    _logger->info("diagnostic: getting configuration file '{}'", *it);
    std::string cfg_path;
    {
      cfg_path = tmp_dir;
      cfg_path.append("/");
      size_t pos(it->find_last_of('/'));
      if (pos != std::string::npos)
        cfg_path.append(it->substr(pos + 1));
      else
        cfg_path.append(*it);
      to_remove.push_back(cfg_path);
      std::ifstream src(*it, std::ios::binary);
      std::ofstream dst(cfg_path, std::ios::binary);
      dst << src.rdbuf();
    }

    // Parse configuration file.
    config::parser parsr;
    config::state conf;
    try {
      _logger->info("diagnostic: reading configuration file.");
      conf = parsr.parse(*it);
    } catch (std::exception const& e) {
      _logger->error("diagnostic: configuration file '{}' parsing failed: {}",
                     *it, e.what());
    }

    // ls.
    _logger->info("diagnostic: getting modules information");
    {
      std::string ls_log_path;
      ls_log_path = tmp_dir;
      ls_log_path.append("/ls_modules_");
      size_t pos(it->find_last_of('/'));
      if (pos != std::string::npos)
        ls_log_path.append(it->substr(pos + 1));
      else
        ls_log_path.append(*it);
      ls_log_path.append(".log");
      to_remove.push_back(ls_log_path);

      std::string cmd{fmt::format("ls -la {} {}", conf.module_directory(),
                                  fmt::join(conf.module_list(), " "))};
      std::string output{misc::exec(cmd)};

      std::ofstream out(ls_log_path);
      out << output;
      out.close();
    }

    // Log files.

    std::string log_path = conf.log_conf().log_path();
    char const* args[]{"tail", "-c", "20000000", log_path.c_str(), nullptr};
    misc::exec_process(args, true);
  }

  // Generate file name if not existing.
  std::string my_out_file;
  if (out_file.empty())
    my_out_file = "cbd-diag.tar.gz";
  else
    my_out_file = out_file;

  // Create tarball.
  _logger->info("diagnostic: creating tarball '{}'", my_out_file);
  {
    std::string cmd{fmt::format("tar czf {} {}", my_out_file, tmp_dir)};
    std::string output{misc::exec(cmd)};
  }

  // Clean temporary directory.
  for (const auto& f : to_remove)
    ::remove(f.c_str());
  ::rmdir(tmp_dir.c_str());
}
