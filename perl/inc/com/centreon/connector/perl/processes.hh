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

#ifndef CCC_PERL_PROCESSES_HH_
# define CCC_PERL_PROCESSES_HH_

# include <map>
# include <sys/types.h>
# include <utility>
# include "com/centreon/connector/perl/process.hh"

namespace                           com {
  namespace                         centreon {
    namespace                       connector {
      namespace                     perl {
        /**
         *  @class processes processes.hh
         *  @brief Process list.
         *
         *  List of processes to monitor for I/O.
         */
        class                       processes {
         private:
          pid_t                     _pid;
          std::map<pid_t, process*> _set;
                                    processes();
                                    processes(processes const& p);
          processes&                operator=(processes const& p);

         public:
                                    ~processes();
          process*&                 operator[](pid_t key);
          std::map<pid_t, process*>::iterator
                                    begin();
          bool                      empty() const;
          std::map<pid_t, process*>::iterator
                                    end();
          void                      erase(pid_t key);
          static processes&         instance();
          unsigned int              size() const;
        };
      }
    }
  }
}

#endif /* !CCC_PERL_PROCESSES_HH_ */
