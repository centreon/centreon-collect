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

#ifndef CCCS_MULTIPLEXER_HH
#  define CCCS_MULTIPLEXER_HH

#  include <memory>
#  include "com/centreon/handle_manager.hh"
#  include "com/centreon/task_manager.hh"
#  include "com/centreon/connector/ssh/namespace.hh"

CCCS_BEGIN()

/**
 *  @class multiplexer multiplexer.hh "com/centreon/connector/ssh/multiplexer.hh"
 *  @brief Multiplexing class.
 *
 *  Singleton that aggregates multiplexing features such as file
 *  descriptor monitoring and task execution.
 */
class                 multiplexer : public com::centreon::task_manager,
                                    public com::centreon::handle_manager {
public:
                      ~multiplexer() throw ();
  static multiplexer& instance() throw ();
  static void         load();
  static void         unload();

private:
                      multiplexer();
                      multiplexer(multiplexer const& m);
  multiplexer&        operator=(multiplexer const& m);

  static std::auto_ptr<multiplexer>
                      _instance;
};

CCCS_END()

#endif // !CCCS_MULTIPLEXER_HH
