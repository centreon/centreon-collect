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

#ifndef CCCS_COMMANDER_HH
#  define CCCS_COMMANDER_HH

#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/handle_listener.hh"
#  include "com/centreon/io/standard_input.hh"
#  include "com/centreon/io/standard_output.hh"

CCCS_BEGIN()

/**
 *  @class commander commander.hh "com/centreon/connector/ssh/commander.hh"
 *  @brief Handle command from upstairs.
 *
 *  Handle check execution command from the monitoring engine.
 */
class        commander : public handle_listener {
public:
             commander();
             ~commander() throw ();
  void       close(handle& h);
  void       error(handle& h);
  void       read(handle& h);
  void       reg();
  void       unreg(bool all = true);
  bool       want_read(handle& h);
  bool       want_write(handle& h);
  void       write(handle& h);

private:
             commander(commander const& c);
  commander& operator=(commander const& c);

  io::standard_input
             _si;
  io::standard_output
             _so;
};

CCCS_END()

#endif // !CCCS_COMMANDER_HH
