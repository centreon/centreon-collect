/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/check_options.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon::misc;

/**
 *  Default constructor.
 *
 *  @param[in] command_line  The connector command line.
 */
check_options::check_options(std::string const& command_line)
  : get_options() {
  _arguments['b'] = argument(
                      "-icmp-bytes-send",
                      'b',
                      "Number of icmp data bytes to send " \
                      "(default \"40\").",
                      true,
                      true,
                      "40");
  _arguments['c'] = argument(
                      "critical",
                      'c',
                      "Critical threshold (default \"500.0ms,80%\").",
                      true,
                      true,
                      "500.0,80%");
  _arguments['H'] = argument(
                      "host",
                      'H',
                      "Specify a target.",
                      true);
  _arguments['i'] = argument(
                      "max-packet-interval",
                      'i',
                      "Max packet interval (default \"80ms\").",
                      true,
                      true,
                      "80");
  _arguments['I'] = argument(
                      "max-target-interval",
                      'I',
                      "Max target interval (default \"0ms\").",
                      true,
                      true,
                      "0");
  _arguments['l'] = argument(
                      "ttl",
                      'l',
                      "TTL on outgoing packets (default \"64\").",
                      true,
                      true,
                      "64");
  _arguments['m'] = argument(
                      "min-hosts-alive",
                      'm',
                      "Number of alive hosts required for success " \
                      "(default \"-1\").",
                      true,
                      true,
                      "-1");
  _arguments['n'] = argument(
                      "packets-number",
                      'n',
                      "Number of packets to send (default \"5\").",
                      true,
                      true,
                      "5");
  _arguments['s'] = argument(
                      "source",
                      's',
                      "Specify a source IP address or device name.",
                      true);
  _arguments['t'] = argument(
                      "timeout",
                      't',
                      "not used (define into Centreon Engine)",
                      true);
  _arguments['w'] = argument(
                      "warning",
                      'w',
                      "Warning threshold (default \"200.0ms,40%\")",
                      true,
                      true,
                      "200.0,40%");
  _parse_arguments(command_line);
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
check_options::check_options(check_options const& right)
  : get_options(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
check_options::~check_options() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
check_options& check_options::operator=(check_options const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
check_options& check_options::_internal_copy(check_options const& right) {
  if (this != &right) {
    get_options::_internal_copy(right);
  }
  return (*this);
}
