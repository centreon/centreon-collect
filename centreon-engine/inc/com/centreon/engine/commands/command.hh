/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_COMMANDS_COMMAND_HH
#  define CCE_COMMANDS_COMMAND_HH

#  include <QMutex>
#  include <QObject>
#  include <string>
#  include "com/centreon/engine/commands/result.hh"
#  include "com/centreon/engine/macros.hh"

namespace                        com {
  namespace                      centreon {
    namespace                    engine {
      namespace                  commands {
        /**
         *  @class command command.hh
         *  @brief Execute command and send the result.
         *
         *  Command execute a command line with their arguments and send the
         *  result by a signal.
         */
        class                    command : public QObject {
          Q_OBJECT

        public:
                                 command(
                                   std::string const& name,
                                   std::string const& command_line);
          virtual                ~command() throw ();
          bool                   operator==(
                                   command const& right) const throw ();
          bool                   operator!=(
                                   command const& right) const throw ();
          virtual command*       clone() const = 0;
          virtual std::string const&
                                 get_command_line() const throw ();
          virtual std::string const&
                                 get_name() const throw ();
          virtual std::string    process_cmd(
                                   nagios_macros* macros) const;
          virtual unsigned long  run(
                                   std::string const& processed_cmd,
                                   nagios_macros const& macors,
                                   unsigned int timeout) = 0;
          virtual void           run(
                                   std::string const& process_cmd,
                                   nagios_macros const& macros,
                                   unsigned int timeout,
                                   result& res) = 0;
          virtual void           set_command_line(
                                   std::string const& command_line);
          virtual void           set_name(std::string const& name);

        signals:
          void                   command_executed(
                                   cce_commands_result const& res);
          void                   name_changed(
                                   std::string const& old_name,
                                   std::string const& new_name);

        protected:
                                 command(command const& right);
          command&               operator=(command const& right);
          static unsigned long   get_uniq_id();

          std::string            _command_line;
          std::string            _name;

        private:
          static unsigned long   _id;
          static QMutex          _mtx;
        };
      }
    }
  }
}

#endif // !CCE_COMMANDS_COMMAND_HH
