/*
** Copyright 2011      Merethis
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
# define CCE_COMMANDS_COMMAND_HH

# include <QObject>
# include <QString>

# include "macros.hh"
# include "commands/result.hh"

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
	                         command(QString const& name,
					 QString const& command_line);
	  virtual                ~command() throw();

	  bool                   operator==(command const& right) const throw();
	  bool                   operator!=(command const& right) const throw();

	  virtual command*       clone() const = 0;

	  virtual QString const& get_name() const throw();
	  virtual QString const& get_command_line() const throw();

	  virtual QString        process_cmd(nagios_macros* macros) const;

	  virtual unsigned long  run(QString const& processed_cmd,
				     nagios_macros const& macors,
				     int timeout) = 0;

	  virtual void           run(QString const& process_cmd,
				     nagios_macros const& macros,
				     int timeout,
				     result& res) = 0;

	  virtual void           set_name(QString const& name);
	  virtual void           set_command_line(QString const& command_line);

	signals:
	  void                   command_executed(commands::result const& res);
	  void                   name_changed(QString const& old_name,
					      QString const& new_name);

	protected:
	  static unsigned long   _id;

	                         command(command const& right);
	  command&               operator=(command const& right);

	  QString                _name;
	  QString                _command_line;
	};
      }
    }
  }
}

#endif // !CCE_COMMANDS_COMMAND_HH
