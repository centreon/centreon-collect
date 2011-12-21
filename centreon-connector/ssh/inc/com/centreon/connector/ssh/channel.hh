/*
** Copyright 2011 Merethis
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

#ifndef CCCS_CHANNEL_HH
#  define CCCS_CHANNEL_HH

#  include <libssh2.h>
#  include <string>
#  include <time.h>
#  include "com/centreon/connector/ssh/check_result.hh"
#  include "com/centreon/connector/ssh/namespace.hh"

CCCS_BEGIN()

/**
 *  @class channel channel.hh "com/centreon/connector/ssh/channel.hh"
 *  @brief SSH channel used to execute a command.
 *
 *  The channel class represents SSH channels used to execute
 *  commands on the remote host.
 */
class                channel {
public:
                     channel(
                       LIBSSH2_SESSION* sess,
                       std::string const& cmd,
                       unsigned long long cmd_id);
                     ~channel();
  bool               run(check_result& cr);

private:
  enum               e_step {
    chan_open = 1,
    chan_exec,
    chan_read,
    chan_close
  };

                     channel(channel const& c);
  channel&           operator=(channel const& c);
  bool               _close(check_result& cr);
  bool               _exec();
  bool               _open();
  bool               _read();

  LIBSSH2_CHANNEL*   _channel;
  std::string        _cmd;
  unsigned long long _cmd_id;
  LIBSSH2_SESSION*   _session;
  std::string        _stderr;
  std::string        _stdout;
  e_step             _step;
};

CCCS_END()

#endif // !CCCS_CHANNEL_HH
