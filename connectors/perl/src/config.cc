/*
** Copyright 2026 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/
#include <sys/resource.h>
#include <boost/program_options.hpp>

#include "com/centreon/connector/perl/config.hh"

using namespace com::centreon::connector::perl;
namespace po = boost::program_options;

config::config(int argc, char** argv) {
  po::options_description desc("Allowed options");

  struct rlimit rlim;
  std::string max_opened_option_help;
  if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
    max_opened_option_help = fmt::format(
        "max number of file descriptors opened by connector default:{}",
        rlim.rlim_cur);
  } else {
    max_opened_option_help =
        "max number of file descriptors opened by connector";
  }

  // clang-format off
  desc.add_options()
    ("help,h", "Print help and exit")
    ("code,c", po::value<std::string>(), "Argument is some Perl code that will be executed by the embedded interpreter.")
    ("debug,d","If this flag is specified, print all logs messages.")
    ("version,v","Print software version and exit.")
    ("log-file,l", po::value<std::string>(),"Specifies the log file (default: stderr).")
    ("log-level", po::value<std::string>()->default_value("info"),"error, info, debug or trace")
    ("test-file,x", po::value<std::string>(),"Specifies the file used instead of stdin.")
    ("max-child", po::value<unsigned>()->default_value(64), "Max number of child process")
    ("min-free-memory", po::value<uint64_t>()->default_value(500), "If free system memory becomes lower than this threshold in Mo, some child processes are killed")
    ("max-opened-fd", po::value<unsigned>(), max_opened_option_help.c_str())
    ("child-max-memory-increase-percent", po::value<unsigned>()->default_value(10), "If memory used by a child process has increased more than this threshold between first and last check, it is killed")
    ("child-max-fd-increase-percent", po::value<unsigned>()->default_value(10), "If the number of file descriptors opened by a child process has increased more than this threshold between first and last check, it is killed")
    ("child-max-thread", po::value<unsigned>()->default_value(10), "If a child process has created more threads than this threshold, it is killed")
    ("child-max-reuse-script", po::value<unsigned>()->default_value(1), "Some perl scripts are not designed to be reused in the same process many times, so if a child process has been used more than this threshold, it is killed")
    ("idle-child-ttl", po::value<unsigned>()->default_value(15), "When a child process has performed no checks for longer than this duration in minutes, it is killed.");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  std::string test_file_path;

  if (vm.count("help")) {
    std::cout << desc << R"(
Per-command overrides:
  Four of the global limits can be overridden on a per-check basis by
  inserting keyword/value pairs directly in the check command line, between
  the script path and the script's own arguments.  Each keyword must be
  followed by a numeric value:

    child-max-reuse-script <N>
        Override --child-max-reuse-script for this command only.

    child-max-memory-increase-percent <N>
        Override --child-max-memory-increase-percent for this command only.

    child-max-fd-increase-percent <N>
        Override --child-max-fd-increase-percent for this command only.

    child-max-thread <N>
        Override --child-max-thread for this command only.

  Example:
    /usr/lib/nagios/plugins/check_something.pl --child-max-reuse-script=5 --child-max-thread=20 --arg1
)" << std::endl;
    _need_to_stop = true;
    return;
  }
  if (vm.count("version")) {
    std::cout << "Centreon Perl Connector " << CENTREON_CONNECTOR_VERSION
              << std::endl;
    _need_to_stop = true;
    return;
  }
  if (vm.count("test-file")) {
    test_file_path = vm["test-file"].as<std::string>();
  }
  if (vm.count("log-file")) {
    _log_file_path = vm["log-file"].as<std::string>();
  }
  if (vm.count("debug")) {
    _log_level = spdlog::level::level_enum::trace;
  }
  if (vm.count("code")) {
    _code = vm["code"].as<std::string>();
  }
  if (vm.count("log-level")) {
    _log_level = spdlog::level::from_str(vm["log-level"].as<std::string>());
  }
  if (vm.count("max-child")) {
    _max_child = vm["max-child"].as<unsigned>();
  }
  if (vm.count("min-free-memory")) {
    _min_free_memory = vm["min-free-memory"].as<uint64_t>() * 1024 * 1024;
  }
  if (vm.count("max-opened-fd")) {
    _max_opened_fd = vm["max-opened-fd"].as<unsigned>();
  }
  if (vm.count("child-max-memory-increase-percent")) {
    _child_max_memory_increase_percent =
        vm["child-max-memory-increase-percent"].as<unsigned>();
  }
  if (vm.count("child-max-fd-increase-percent")) {
    _child_max_fd_increase_percent =
        vm["child-max-fd-increase-percent"].as<unsigned>();
  }
  if (vm.count("child-max-thread")) {
    _child_max_thread = vm["child-max-thread"].as<unsigned>();
  }
  if (vm.count("child-max-reuse-script")) {
    _child_max_reuse_script = vm["child-max-reuse-script"].as<unsigned>();
  }
  if (vm.count("idle-child-ttl")) {
    _minute_idle_check_child_ttl = vm["idle-child-ttl"].as<unsigned>();
  }
}
