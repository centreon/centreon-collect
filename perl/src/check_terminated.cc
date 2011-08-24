/*
** Copyright 2011 Merethis
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <sstream>
#include <sys/wait.h>
#include <time.h>
#include "com/centreon/connector/perl/check_terminated.hh"
#include "com/centreon/connector/perl/main_io.hh"
#include "com/centreon/connector/perl/processes.hh"

/**
 *  Check for terminated processes and send results back to parent.
 */
void com::centreon::connector::perl::check_terminated() {
  // Loop on all terminated processes.
  int status;
  for (pid_t child(waitpid((pid_t)-1, &status, WNOHANG));
       (child != 0) && (child != -1);
       child = waitpid((pid_t)-1, &status, WNOHANG)) {
    // Get process data.
    process* data(processes::instance()[child]);

    if (data) {
      // Forge response packet.
      std::ostringstream packet;
      // Packet ID.
      packet << "3";
      packet.put('\0');
      // Command ID.
      packet << data->cmd();
      packet.put('\0');
      // Executed.
      packet << "1";
      packet.put('\0');
      // Exit code.
      packet << WEXITSTATUS(status);
      packet.put('\0');
      // End time.
      packet << time(NULL);
      packet.put('\0');
      // Error output.
      packet << data->err();
      packet.put('\0');
      // Standard output.
      packet << data->out();
      for (unsigned int i = 0; i < 4; ++i)
        packet.put('\0');

      // Send response packet.
      main_io::instance().write(packet.str());
    }

    // Remove process from list.
    processes::instance().erase(child);
  }
  return ;
}
