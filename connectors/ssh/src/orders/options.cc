/*
** Copyright 2011-2013 Centreon
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

#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif  // Windows or POSIX.
#include <getopt.h>
#include <cstdlib>
#include "absl/strings/numbers.h"
#include "com/centreon/connector/ssh/orders/options.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/misc/command_line.hh"

using namespace com::centreon::connector::orders;

static char const* optstr = "1246a:C:E:fhH:i:l:n:o:O:p:qs:S:t:vV";
static struct option optlong[] = {
    {"authentication", required_argument, nullptr, 'a'},
    {"command", required_argument, nullptr, 'C'},
    {"fork", no_argument, nullptr, 'f'},
    {"help", no_argument, nullptr, 'h'},
    {"hostname", required_argument, nullptr, 'H'},
    {"identity", required_argument, nullptr, 'i'},
    {"logname", required_argument, nullptr, 'l'},
    {"name", required_argument, nullptr, 'n'},
    {"output", required_argument, nullptr, 'O'},
    {"port", required_argument, nullptr, 'p'},
    {"proto1", no_argument, nullptr, '1'},
    {"proto2", no_argument, nullptr, '2'},
    {"quiet", no_argument, nullptr, 'q'},
    {"services", required_argument, nullptr, 's'},
    {"skip", optional_argument, nullptr, 'S'},
    {"skip-stderr", optional_argument, nullptr, 'E'},
    {"skip-stdout", optional_argument, nullptr, 'S'},
    {"ssh-option", required_argument, nullptr, 'o'},
    {"timeout", required_argument, nullptr, 't'},
    {"use-ipv4", no_argument, nullptr, '4'},
    {"use-ipv6", no_argument, nullptr, '6'},
    {"verbose", no_argument, nullptr, 'v'},
    {"version", no_argument, nullptr, 'V'},
    {nullptr, no_argument, nullptr, 0}};

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
options::options(std::string const& cmdline)
    : _ip_protocol(ip_v4),
      _port(22),
      _skip_stderr(-1),
      _skip_stdout(-1),
      _timeout(0) {
  if (!cmdline.empty())
    parse(cmdline);
}

/**
 *  Get the authentication password.
 *
 *  @return The password.
 */
std::string const& options::get_authentication() const noexcept {
  return (_authentication);
}

/**
 *  Get command to execute on the remote machine.
 *
 *  @return The command to execute.
 */
std::list<std::string> const& options::get_commands() const noexcept {
  return (_commands);
}

/**
 *  Get host name, IP Address.
 *
 *  @return A string.
 */
std::string const& options::get_host() const noexcept {
  return (_host);
}

/**
 *  Get identity of an authorized key.
 *
 *  @return A file path.
 */
std::string const& options::get_identity_file() const noexcept {
  return (_identity_file);
}

/**
 *  Get ip connection protocol.
 *
 *  @return The version (v4 or v6).
 */
options::ip_protocol options::get_ip_protocol() const noexcept {
  return (_ip_protocol);
}

/**
 *  Get port number for ssh connection.
 *
 *  @return The port number.
 */
unsigned short options::get_port() const noexcept {
  return (_port);
}

/**
 *  Get timeout connection.
 *
 *  @return The timeout.
 */
unsigned int options::get_timeout() const noexcept {
  return (_timeout);
}

/**
 *  Get user use to connect on remote host.
 *
 *  @return The user name.
 */
std::string const& options::get_user() const noexcept {
  return (_user);
}

/**
 *  Get the help string.
 *
 *  @return A string.
 */
std::string options::help() {
  std::string help;
  help =
      "  -1, --proto1:         This option is not supported.\n"
      "  -2, --proto2:         Tell ssh to use Protocol 2.\n"
      "  -4, --use-ipv4:       Enable IPv4 connection.\n"
      "  -6, --use-ipv6:       Enable IPv6 connection.\n"
      "  -a, --authentication: Authentication password.\n"
      "  -C, --command:        Command to execute on the remote machine.\n"
      "  -E, --skip-stderr:    Ignore all or first n lines on STDERR.\n"
      "  -f, --fork:           This option is not supported.\n"
      "  -h, --help:           Not used.\n"
      "  -H, --hostname:       Host name, IP Address.\n"
      "  -i, --identity:       Identity of an authorized key.\n"
      "  -l, --logname:        SSH user name on remote host.\n"
      "  -n, --name:           This option is not supported.\n"
      "  -o, --ssh-option:     This option is not supported.\n"
      "  -O, --output:         This option is not supported.\n"
      "  -p, --port:           Port number (default: 22).\n"
      "  -q, --quiet:          Not used.\n"
      "  -s, --services:       This option is not supported.\n"
      "  -S, --skip-stdout:    Ignore all or first n lines on STDOUT.\n"
      "  -t, --timeout:        Seconds before connection times out (default: "
      "10).\n"
      "  -v, --verbose:        Not used.\n"
      "  -V, --version:        Not used.\n";
  return (help);
}

/**
 *  Parse command line.
 *
 *  @param[in] cmdline The command line to get options.
 */
void options::parse(std::string const& cmdline) {
  misc::command_line cmd(cmdline);
  int ac(cmd.get_argc());
  char* const* av(cmd.get_argv());

  optind = 0;  // Reset optind to parse arguments.
  opterr = 0;  // Disable output messages.

  int c;
  while ((c = getopt_long(ac, av, optstr, optlong, nullptr)) > 0) {
    switch (c) {
      case 'H':  // Set host name or IP address.
        _host = optarg;
        break;

      case 'C':  // Set command line to execute.
        _commands.emplace_back(optarg);
        break;

      case 'a':  // Set user.
        _authentication = optarg;
        break;

      case 'l':  // Set logging name.
        _user = optarg;
        break;

      case 'p':  // Set port.
      {
        unsigned int temp;
        if (!absl::SimpleAtoi(optarg, &temp)) {
          throw basic_error() << "the argument '" << optarg
                              << "' must be an unsigned short integer";
        }
        if (temp > 65535) {
          throw basic_error() << "the argument '" << optarg
                              << "' must be an integer between 0 and 65535";
        }
        _port = temp;
      } break;

      case '4':  // Enable IPv4.
        _ip_protocol = ip_v4;
        break;

      case '6':  // Enalbe IPv6.
        _ip_protocol = ip_v6;
        break;

      case '1':  // Enable SSH v1.
        throw basic_error() << "'" << c << "' option is not supported";
        break;

      case '2':  // Enable SSH v2.
        // Enabled by default.
        break;

      case 'E':  // Skip stderr.
        if (!optarg)
          _skip_stderr = 0;
        else if (!absl::SimpleAtoi(optarg, &_skip_stderr)) {
          throw basic_error()
              << "the argument '" << optarg << "' must be an integer";
        }
        break;

      case 'f':  // Fork ssh.
        throw basic_error() << "'" << c << "' option is not supported";
        break;

      case 'i':  // Set Identity file.
        _identity_file = optarg;
        break;

      case 'n':  // Host name for monitoring engine.
        throw basic_error() << "'" << c << "' option is not supported";
        break;

      case 'o':  // Set ssh-option.
        throw basic_error() << "'" << c << "' option is not supported";
        break;

      case 'O':  // Set output file.
        throw basic_error() << "'" << c << "' option is not supported";
        break;

      case 's':  // Services.
        throw basic_error() << "'" << c << "' option is not supported";
        break;

      case 'S':  // Skip stdout.
        if (!optarg)
          _skip_stdout = 0;
        else if (!absl::SimpleAtoi(optarg, &_skip_stdout)) {
          throw basic_error()
              << "the argument '" << optarg << "' must be an integer";
        }
        break;

      case 't':  // Set timeout.
        if (!absl::SimpleAtoi(optarg, &_timeout)) {
          throw basic_error()
              << "the argument '" << optarg << "' must be an unsigned integer";
        }
        break;

      case 'h':  // Help.
      case 'q':  // Quiet.
      case 'v':  // Verbose.
      case 'V':  // Version.
        // These options are ignored.
        break;

      case '?':  // Missing argument.
        throw basic_error() << "option '" << c << "' requires an argument";

      default:  // Unknown argument.
        throw basic_error() << "unrecognized option '" << c << "'";
    }
  }
  if (_user.empty())
    _user = _get_user_name();
}

/**
 *  Disable error output.
 *
 *  @return 0 to drop all data, n to keep n line, otherwise -1.
 */
int options::skip_stderr() const noexcept {
  return _skip_stderr;
}

/**
 *  Disable standard output.
 *
 *  @return 0 to drop all data, n to keep n line, otherwise -1.
 */
int options::skip_stdout() const noexcept {
  return _skip_stdout;
}

/**
 *  Get the current login name.
 *
 *  @return The current login name.
 */
std::string options::_get_user_name() {
  errno = 0;
  passwd* pwd(getpwuid(getuid()));
  if (!pwd || !pwd->pw_name) {
    char const* msg(strerror(errno));
    throw basic_error() << "cannot get current user name: " << msg;
  }
  return pwd->pw_name;
}
