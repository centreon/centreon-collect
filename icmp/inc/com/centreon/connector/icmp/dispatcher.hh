/*
** Copyright 2011 Merethis
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

#ifndef CCC_DISPATCHER_HH
# define CCC_DISPATCHER_HH

# include <QThread>
# include <QString>
# include <QSharedPointer>
# include <QThreadPool>
# include <QTextStream>
# include "commands/connector/request.hh"

namespace               com {
  namespace             centreon {
    namespace           connector {
      namespace         icmp {
	/**
	 *  @class dispatcher dispatcher.hh
	 *  @brief Dispatch request from engine.
	 *
	 *  Dispatch request from engine and start execute
	 *  command.
	 */
	class           dispatcher : public QThread {
	  Q_OBJECT
	public:
	                dispatcher(int argc, char** argv);
	                ~dispatcher() throw();

	protected:
	  void          run();

	private slots:
	  void          _init();
	  void          _finish(unsigned long cmd_id, QString const& output, int exit_code);

	private:
	  QSharedPointer<engine::commands::connector::request>
	                _wait_request();

	  QThreadPool   _th_pool;
	  QTextStream   _stream;
	  char**        _argv;
	  int           _argc;

	  static const unsigned int ENGINE_MAJOR = 1;
	  static const unsigned int ENGINE_MINOR = 0;
	};
      }
    }
  }
}

#endif // !CCC_DISPATCHER_HH
