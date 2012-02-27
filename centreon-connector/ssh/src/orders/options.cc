/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/misc/command_line.hh"
#include "com/centreon/connector/ssh/orders/options.hh"

using namespace com::centreon::connector::ssh::orders;

static char const* optstr = "1246a:c:C:E:fhH:i:l:n:o:O:p:qs:S:t:vVw:";
static struct option optlong[] = {
  { "authentication", required_argument, NULL, 'a' },
  { "command",        required_argument, NULL, 'C' },
  { "fork",           no_argument,       NULL, 'f' },
  { "help",           no_argument,       NULL, 'h' },
  { "hostname",       required_argument, NULL, 'H' },
  { "identity",       required_argument, NULL, 'i' },
  { "logname",        required_argument, NULL, 'l' },
  { "name",           required_argument, NULL, 'n' },
  { "output",         required_argument, NULL, 'O' },
  { "port",           required_argument, NULL, 'p' },
  { "proto1",         no_argument,       NULL, '1' },
  { "proto2",         no_argument,       NULL, '2' },
  { "quiet",          no_argument,       NULL, 'q' },
  { "services",       required_argument, NULL, 's' },
  { "skip",           optional_argument, NULL, 'S' },
  { "skip-stderr",    optional_argument, NULL, 'E' },
  { "skip-stdout",    optional_argument, NULL, 'S' },
  { "ssh-option",     required_argument, NULL, 'o' },
  { "timeout",        required_argument, NULL, 't' },
  { "use-ipv4",       no_argument,       NULL, '4' },
  { "use-ipv6",       no_argument,       NULL, '6' },
  { "verbose",        no_argument,       NULL, 'v' },
  { "version",        no_argument,       NULL, 'V' },
  { NULL,             no_argument,       NULL, 0   }
};

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
 *  Copy constructor.
 *
 *  @param[in] p Object to copy.
 */
options::options(options const& right) {
  _copy(right);
}

/**
 *  Destructor.
 */
options::~options() throw () {

}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
options& options::operator=(options const& right) {
  if (this != &right)
    _copy(right);
  return (*this);
}

/**
 *  Get the authentication password.
 *
 *  @return The password.
 */
std::string const& options::get_authentication() const throw () {
  return (_authentication);
}

/**
 *  Get command to execute on the remote machine.
 *
 *  @return The command to execute.
 */
std::list<std::string> const& options::get_commands() const throw () {
  return (_commands);
}

/**
 *  Get host name, IP Address.
 *
 *  @return A string.
 */
std::string const& options::get_host() const throw () {
  return (_host);
}

/**
 *  Get identity of an authorized key.
 *
 *  @return A file path.
 */
std::string const& options::get_identity_file() const throw () {
  return (_identity_file);
}

/**
 *  Get ip connection protocol.
 *
 *  @return The version (v4 or v6).
 */
options::ip_protocol options::get_ip_protocol() const throw () {
  return (_ip_protocol);
}

/**
 *  Get port number for ssh connection.
 *
 *  @return The port number.
 */
unsigned short options::get_port() const throw () {
  return (_port);
}

/**
 *  Get timeout connection.
 *
 *  @return The timeout.
 */
unsigned int options::get_timeout() const throw () {
  return (_timeout);
}

/**
 *  Get user use to connect on remote host.
 *
 *  @return The user name.
 */
std::string const& options::get_user() const throw () {
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
    "  -1, --proto1:         This option is not supported.\n"           \
    "  -2, --proto2:         Tell ssh to use Protocol 2.\n"             \
    "  -4, --use-ipv4:       Enable IPv4 connection.\n"                 \
    "  -6, --use-ipv6:       Enable IPv6 connection.\n"                 \
    "  -a, --authentication: Authentication password.\n"                \
    "  -C, --command:        Command to execute on the remote machine.\n" \
    "  -E, --skip-stderr:    Ignore all or first n lines on STDERR.\n"  \
    "  -f, --fork:           This option is not supported.\n"           \
    "  -h, --help:           Not used.\n"                               \
    "  -H, --hostname:       Host name, IP Address.\n"                  \
    "  -i, --identity:       Identity of an authorized key.\n"          \
    "  -l, --logname:        SSH user name on remote host.\n"           \
    "  -n, --name:           This option is not supported.\n"           \
    "  -o, --ssh-option:     This option is not supported.\n"           \
    "  -O, --output:         This option is not supported.\n"           \
    "  -p, --port:           Port number (default: 22).\n"              \
    "  -q, --quiet:          Not used.\n"                               \
    "  -s, --services:       This option is not supported.\n"           \
    "  -S, --skip-stdout:    Ignore all or first n lines on STDOUT.\n"  \
    "  -t, --timeout:        Seconds before connection times out (default: 10).\n" \
    "  -v, --verbose:        Not used.\n"                               \
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
  char** av(cmd.get_argv());

  char c;
  while ((c = getopt_long(ac, av, optstr, optlong, NULL)) > 0) {
    switch (c) {
    case 'H': // Set Host name or IP address.
      _host = optarg;
      break;

    case 'C': // Set command line to execute.
      _commands.push_back(optarg);
      break;

    case 'a': // Set user.
      _authentication = optarg;
      break;

    case 'l': // Set loggin name.
      _user = optarg;
      break;

    case 'p': // Set port.
      _port = atoi(optarg);
      break;

    case '4': // Enable IPv4.
      _ip_protocol = ip_v4;
      break;

    case '6': // Enalbe IPv6.
      _ip_protocol = ip_v6;
      break;

    case '1': // Enable ssh protocole v1.
      throw (basic_error() << "'" << c
             << "' option is not supported");
      break;

    case '2': // Enable ssh protocole v2.
      // Enable by default.
      break;

    case 'E': // Skip stderr.
      _skip_stderr = atoi(optarg);
      break;

    case 'f': // Fork ssh.
      throw (basic_error() << "'" << c
             << "' option is not supported");
      break;

    case 'i': // Set Identity file.
      _identity_file = optarg;
      break;

    case 'n': // Host name for monitoring engine.
      throw (basic_error() << "'" << c
             << "' option is not supported");
      break;

    case 'o': // Set ssh-option.
      throw (basic_error() << "'" << c
             << "' option is not supported");
      break;

    case 'O': // Set output file.
      throw (basic_error() << "'" << c
             << "' option is not supported");
      break;

    case 's': // services.
      throw (basic_error() << "'" << c
             << "' option is not supported");
      break;

    case 'S': // Skip stdout.
      _skip_stdout = atoi(optarg);
      break;

    case 't': // Set timeout.
      _timeout = atoi(optarg);
      break;

    case 'h': // Help.
    case 'q': // Quiet.
    case 'v': // Verbose.
    case 'V': // Version.
      // These options are ignore.
      break;

    case '?': // Missing argument.
      throw (basic_error() << "option '" << c
             << "' requires an argument");

    default:  // Unknown argument.
      throw (basic_error() << "unrecognized option '" << c << "'");
    }
  }
}

/**
 *  Disable error output.
 *
 *  @return 0 to drop all data, n to keep n line, otherwise -1.
 */
int options::skip_stderr() const throw () {
  return (_skip_stderr);
}

/**
 *  Disable standard output.
 *
 *  @return 0 to drop all data, n to keep n line, otherwise -1.
 */
int options::skip_stdout() const throw () {
  return (_skip_stdout);
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] right Object to copy.
 */
void options::_copy(options const& right) {
  _authentication = right._authentication;
  _commands = right._commands;
  _host = right._host;
  _identity_file = right._identity_file;
  _ip_protocol = right._ip_protocol;
  _port = right._port;
  _skip_stderr = right._skip_stderr;
  _skip_stdout = right._skip_stdout;
  _timeout = right._timeout;
  _user = right._user;
}
