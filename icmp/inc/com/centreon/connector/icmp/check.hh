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

#ifndef CCC_ICMP_CHECK_HH
#  define CCC_ICMP_CHECK_HH

#  include <string>
#  include "com/centreon/connector/icmp/host.hh"
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/logging/temp_logger.hh"
#  include "com/centreon/task.hh"

CCC_ICMP_BEGIN()

/**
 *  @class check check.hh "com/centreon/connector/icmp/check.hh"
 *  @brief Check is a task run by check dispatcher. Check as all
 *  information on the command.
 *
 *  This class provid all information on the request send by
 *  Centreon engine for make check icmp.
 */
class                     check {
public:
                          check(
                            unsigned int command_id = 0,
                            std::string const& command_line = "");
                          check(check const& right);
                          ~check() throw ();
  check&                  operator=(check const& right);
  void                    host_was_checked() throw ();
  unsigned int            get_command_id() const throw ();
  unsigned int            get_current_host_check() const throw ();
  std::list<host*> const& get_hosts() const throw ();
  std::list<host*>&       get_hosts() throw ();
  unsigned int            get_max_completion_time() const throw ();
  unsigned int            get_max_packet_interval() const throw ();
  unsigned int            get_max_target_interval() const throw ();
  int                     get_min_hosts_alive() const throw ();
  unsigned int            get_nb_packet() const throw ();
  unsigned int            get_packet_data_size() const throw ();
  unsigned int            get_critical_packet_lost() const throw ();
  unsigned int            get_critical_roundtrip_avg() const throw ();
  unsigned int            get_warning_packet_lost() const throw ();
  unsigned int            get_warning_roundtrip_avg() const throw ();
  void                    parse();

private:
  check&                  _internal_copy(check const& right);
  static bool             _get_threshold(
                            std::string const& str,
                            unsigned int& packet_lost,
                            unsigned int& roundtrip_average);
  template<typename T>
  static bool             _to_obj(std::string const& str, T& obj);
  static std::string      _trim(std::string str);

  unsigned int            _command_id;
  std::string             _command_line;
  unsigned int            _critical_packet_lost;
  unsigned int            _critical_roundtrip_avg;
  unsigned int            _current_host_check;
  std::list<host*>        _hosts;
  unsigned int            _max_packet_interval;
  unsigned int            _max_target_interval;
  int                     _min_hosts_alive;
  unsigned int            _nb_packet;
  unsigned int            _packet_data_size;
  std::string             _source_address;
  unsigned char           _ttl;
  unsigned int            _warning_packet_lost;
  unsigned int            _warning_roundtrip_avg;
};

logging::temp_logger operator<<(
                       logging::temp_logger log,
                       check const& right);

CCC_ICMP_END()

#endif // !CCC_ICMP_CHECK_HH
