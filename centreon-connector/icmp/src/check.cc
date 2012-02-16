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

#include <algorithm>
#include <ctype.h>
#include <getopt.h>
#include <iterator>
#include <sstream>
#include <wordexp.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"
#include "com/centreon/connector/icmp/check.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon;

static option long_options[] = {
  { "icmp-bytes-send",     1, NULL, 'b' },
  { "critical",            1, NULL, 'c' },
  { "host",                1, NULL, 'H' },
  { "max-packet-interval", 1, NULL, 'i' },
  { "max-target-interval", 1, NULL, 'I' },
  { "ttl",                 1, NULL, 'l' },
  { "min-hosts-alive",     1, NULL, 'm' },
  { "packets-number",      1, NULL, 'n' },
  { "source",              1, NULL, 's' },
  { "timeout",             1, NULL, 't' },
  { "warning",             1, NULL, 'w' },
  { NULL,                  0, NULL,  0  }
};

/**
 *  Default constructor.
 *
 *  @param[in] command_id    The connector command id.
 *  @param[in] command_line  The connector command line.
 */
check::check(unsigned int command_id, std::string const& command_line)
  : _command_id(command_id),
    _command_line("bin " + command_line),
    _critical_packet_lost(80),
    _critical_roundtrip_avg(500000),
    _current_host_check(0),
    _max_packet_interval(80000),
    _max_target_interval(0),
    _min_hosts_alive(-1),
    _nb_packet(5),
    _packet_data_size(68 + 8),
    _ttl(64),
    _warning_packet_lost(40),
    _warning_roundtrip_avg(200000) {
  _max_completion_time
    = _hosts_size * (_nb_packet * (_max_packet_interval
                                   + _critical_roundtrip_avg)
                     + _max_target_interval) + _critical_roundtrip_avg;
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
check::check(check const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
check::~check() throw () {
  for (std::list<host*>::const_iterator
         it(_hosts.begin()), end(_hosts.end());
       it != end;
       ++it)
    delete *it;
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
check& check::operator=(check const& right) {
  return (_internal_copy(right));
}

/**
 *  One more host was checked.
 */
void check::host_was_checked() throw () {
  ++_current_host_check;
}

/**
 *  Get the command id.
 *
 *  @return The command id.
 */
unsigned int check::get_command_id() const throw () {
  return (_command_id);
}

/**
 *  Get the current host check.
 *
 *  @return The current host check.
 */
unsigned int check::get_current_host_check() const throw () {
  return (_current_host_check);
}


/**
 *  Get the hosts list.
 *
 *  @return The hosts list.
 */
std::list<host*> const& check::get_hosts() const throw () {
  return (_hosts);
}

/**
 *  Overload of get_hosts.
 *
 *  @return The hosts list.
 */
std::list<host*>& check::get_hosts() throw () {
  return (_hosts);
}

/**
 *  Get the number of hosts to check.
 *
 *  @return The number of hosts.
 */
size_t check::get_hosts_size() const throw () {
  return (_hosts_size);
}

/**
 *  Get the maximum completion time. This is the maximum time
 *  for this check.
 *
 *  @return The maximum completion time.
 */
unsigned int check::get_max_completion_time() const throw () {
  return (_max_completion_time);
}

/**
 *  Get the maximum packet time between two send for an host
 *  in micosecond.
 *
 *  @return The maximum packet interval.
 */
unsigned int check::get_max_packet_interval() const throw () {
  return (_max_packet_interval);
}

/**
 *  Get the maximum target time to send all packet for one host
 *  in microsecond.
 *
 *  @return The maximum target interval.
 */
unsigned int check::get_max_target_interval() const throw () {
  return (_max_target_interval);
}

/**
 *  Get the number of minimum alive hosts required for success.
 *
 *  @return The minimum hosts alive.
 */
int check::get_min_hosts_alive() const throw () {
  return (_min_hosts_alive);
}

/**
 *  Get the number of packet send for this check.
 *
 *  @return The number of packet.
 */
unsigned int check::get_nb_packet() const throw () {
  return (_nb_packet);
}

/**
 *  Get the size of packet send for this check.
 *
 *  @return The size of packet.
 */
unsigned int check::get_packet_data_size() const throw () {
  return (_packet_data_size);
}

/**
 *  Get the number of critical packet lost.
 *
 *  @return The number of critical packet lost.
 */
unsigned int check::get_critical_packet_lost() const throw () {
  return (_critical_packet_lost);
}

/**
 *  Get the critical roundtrip time average.
 *
 *  @return The roundtrip time average.
 */
unsigned int check::get_critical_roundtrip_avg() const throw () {
  return (_critical_roundtrip_avg);
}

/**
 *  Get the number of warning packet lost.
 *
 *  @return The number of critical packet lost.
 */
unsigned int check::get_warning_packet_lost() const throw () {
  return (_warning_packet_lost);
}

/**
 *  Get the warning roundtrip time average.
 *
 *  @return The roundtrip time average.
 */
unsigned int check::get_warning_roundtrip_avg() const throw () {
  return (_warning_roundtrip_avg);
}

/**
 *  Init check with the command line.
 */
void check::parse() {
  wordexp_t p;
  wordexp(_command_line.c_str(), &p, 0);
  int argc(p.we_wordc);
  char** argv(p.we_wordv);

  try {
    char c;
    while ((c = getopt_long(
                  argc,
                  argv,
                  "b:c:H:i:I:l:m:n:s:t:w:",
                  long_options,
                  NULL)) != -1) {
      switch (c) {
      case 'b': // Number of icmp data bytes to send.
        if (!_to_obj(optarg, _packet_data_size))
          throw (basic_error() << "invalid option 'b' ("
                 << optarg << ")");
        break;

      case 'c': // Critical threshold.
        if (!_get_threshold(
               optarg,
               _critical_packet_lost,
               _critical_roundtrip_avg))
          throw (basic_error() << "invalid option 'c' ("
                 << optarg << ")");
        break;

      case 'H': // Specify a target.
        host::factory(optarg, _hosts);
        break;

      case 'i': // Max packet interval.
        if (!_to_obj(optarg, _max_packet_interval))
          throw (basic_error() << "invalid option 'i' ("
                 << optarg << ")");
        _max_packet_interval *= 1000;
        break;

      case 'I': // Max target interval.
        if (!_to_obj(optarg, _max_target_interval))
          throw (basic_error() << "invalid option 'I' ("
                 << optarg << ")");
        _max_target_interval *= 1000;
        break;

      case 'l': // TTL on outgoing packets.
        if (!_to_obj(optarg, _ttl))
          throw (basic_error() << "invalid option 'l' ("
                 << optarg << ")");
        break;

      case 'm': // Number of alive hosts required for success.
        if (!_to_obj(optarg, _min_hosts_alive))
          throw (basic_error() << "invalid option 'm' ("
                 << optarg << ")");
        break;

      case 'n': // Number of packets to send.
        if (!_to_obj(optarg, _nb_packet))
          throw (basic_error() << "invalid option 'n' ("
                 << optarg << ")");
        break;

      case 's': // Specify a source IP address or device name.
        _source_address = optarg;
        break;

      case 't': // Timeout not used.
        break;

      case 'w': // Warning threshold.
        if (!_get_threshold(
               optarg,
               _warning_packet_lost,
               _warning_roundtrip_avg))
          throw (basic_error() << "invalid option 'w' ("
                 << optarg << ")");
        break;

      case '?': // Missing argument.
        throw (basic_error() << "option '" << c
               << "' requires an argument");

      default:
        throw (basic_error() << "unrecognized option '" << c << "'");
      }
    }
    while (optind < argc)
      host::factory(argv[optind++], _hosts);
    _hosts_size = _hosts.size();
  }
  catch (...) {
    wordfree(&p);
    throw;
  }

  if (_hosts.empty())
    throw (basic_error() << "invalid command line:no host " \
           "was define");

  if (_nb_packet > 20)
    throw (basic_error() << "invalid options 'n' ("
           << _nb_packet << " > 20)");
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
check& check::_internal_copy(check const& right) {
  if (this != &right) {
    _command_id = right._command_id;
    _command_line = right._command_line;
    _critical_packet_lost = right._critical_packet_lost;
    _critical_roundtrip_avg = right._critical_roundtrip_avg;
    _hosts = right._hosts;
    _max_completion_time = right._max_completion_time;
    _max_packet_interval = right._max_packet_interval;
    _max_target_interval = right._max_target_interval;
    _min_hosts_alive = right._min_hosts_alive;
    _current_host_check = right._current_host_check;
    _nb_packet = right._nb_packet;
    _packet_data_size = right._packet_data_size;
    _source_address = right._source_address;
    _ttl = right._ttl;
    _warning_packet_lost = right._warning_packet_lost;
    _warning_roundtrip_avg = right._warning_roundtrip_avg;
  }
  return (*this);
}

/**
 *  Convert a string into threshold.
 *
 *  @param[in]  str                The string to convert.
 *  @param[out] packet_lost        The packet lost value to fill.
 *  @param[out] roundtrip_average  The roundtrip average value to fill.
 *
 *  @return True on success, otherwise false.
 */
bool check::_get_threshold(
              std::string const& str,
              unsigned int& packet_lost,
              unsigned int& roundtrip_average) {
  size_t pos(str.find(',', 0));
  if (str.size() < 3 || pos == std::string::npos)
    return (false);
  std::string pl(_trim(str.substr(pos + 1)));
  if (pl[pl.size() - 1] == '%')
    pl.erase(pl.size() - 1);
  if (!_to_obj(pl, packet_lost))
    return (false);

  std::string rta(_trim(str.substr(0, pos)));
  unsigned int factor(1000);
  if (isalpha(rta[rta.size() - 1])) {
    char unit = rta[rta.size() - 1];
    rta.erase(rta.size() - 1);
    if (unit == 'u')
      factor = 1;
    else if (unit == 'm')
      factor = 1000;
    else if (unit == 's')
      factor = 1000000;
    else
      return (false);
  }
  pos = rta.find('.', 0);

  unsigned int first(0);
  unsigned int second(0);
  if (pos != std::string::npos) {
    if (!_to_obj(rta.substr(0, pos), first))
      return (false);
    if (!_to_obj(rta.substr(pos + 1), second))
      return (false);
  }
  else if (!_to_obj(rta, first))
    return (false);

  while (second >= factor)
    second /= 10;

  roundtrip_average = first * factor + second * (factor / 10);
  return (true);
}

/**
 *  Convert a string into an other object.
 *
 *  @param[in]  str  The string to convert.
 *  @param[out] obj  Fill obj with the result of the convertion.
 *
 *  @return True on success, otherwise false.
 */
template<typename T>
bool check::_to_obj(std::string const& str, T& obj) {
  std::istringstream iss(str);
  return ((iss >> obj) != 0);
}

/**
 *  Delete begin and ending whitespace from a string.
 *
 *  @param[in] str  The sting to trim.
 *
 *  @return The trimmed string.
 */
std::string check::_trim(std::string str) {
  static char const* tab("\t\n\r");

  str.erase(0, str.find_first_not_of(tab));
  str.erase(str.find_last_not_of(tab) + 1);
  return (str);
}

/**
 *  Overload of redirectoion operator for temp_logger. Allow to log
 *  check.
 *
 *  @param[out] log    The temp_logger to write data.
 *  @param[in]  right  The check to log.
 */
logging::temp_logger connector::icmp::operator<<(
                                        logging::temp_logger log,
                                        check const& right) {
  log << "check (" << &right << ") {\n"
      << "  command_id:             " << right.get_command_id() << "\n"
      << "  current host check:     " << right.get_current_host_check() << "\n"
      << "  max_completion_time:    " << right.get_max_completion_time() << "\n"
      << "  max_packet_interval:    " << right.get_max_packet_interval() << "\n"
      << "  max_target_interval:    " << right.get_max_target_interval() << "\n"
      << "  min_hosts_alive:        " << right.get_min_hosts_alive() << "\n"
      << "  nb_packet:              " << right.get_nb_packet() << "\n"
      << "  packet_data_size:       " << right.get_packet_data_size() << "\n"
      << "  critical_packet_lost:   " << right.get_critical_packet_lost() << "\n"
      << "  critical_roundtrip_avg: " << right.get_critical_roundtrip_avg() << "\n"
      << "  warning_packet_lost:    " << right.get_warning_packet_lost() << "\n"
      << "  warning_roundtrip_avg:  " << right.get_warning_roundtrip_avg() << "\n";

  log << "  hosts:\n";
  std::list<host*>const& hosts(right.get_hosts());
  for (std::list<host*>::const_iterator
         it(hosts.begin()), end(hosts.end());
       it != end;
       ++it)
    log << "    host name:            " << (*it)->get_name()
        << "(" << host::address_to_string((*it)->get_address()) << ")\n";
  log << "}";
  return (log);
}
