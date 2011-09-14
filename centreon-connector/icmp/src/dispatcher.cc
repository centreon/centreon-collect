/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU General Public License
** version 2 as published by the Free Software Foundation.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <QCoreApplication>
#include <QDateTime>
#include "com/centreon/engine/engine.hh"
#include "com/centreon/engine/commands/connector/quit_response.hh"
#include "com/centreon/engine/commands/connector/version_response.hh"
#include "com/centreon/engine/commands/connector/error_response.hh"
#include "com/centreon/engine/commands/connector/execute_response.hh"
#include "com/centreon/engine/commands/connector/execute_query.hh"
#include "com/centreon/engine/commands/connector/request_builder.hh"
#include "com/centreon/connector/icmp/socket_manager.hh"
#include "com/centreon/connector/icmp/check.hh"
#include "com/centreon/connector/icmp/dispatcher.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon::engine;

/**
 *  Default constructor.
 *
 *  @param[in] argc The number of command line arguments.
 *  @param[in] argv The array of command line arguments.
 */
dispatcher::dispatcher(int argc, char** argv)
  : _stream(stdout), _argv(argv), _argc(argc) {
  _th_pool.setExpiryTimeout(-1);
}

/**
 *  Default destructor.
 */
dispatcher::~dispatcher() throw() {
  QCoreApplication::instance()->quit();
}

/**
 *  Run dispatcher in a new thread.
 */
void dispatcher::run() {
  try {
    while (true) {
      QSharedPointer<commands::connector::request> req = _wait_request();

      switch (req->get_id()) {
      case commands::connector::request::version_q: {
	commands::connector::version_response version(ENGINE_MAJOR, ENGINE_MINOR);
	_stream << version.build() << flush;
	_init();
	break;
      }

      case commands::connector::request::execute_q: {
	commands::connector::execute_query const* exec_query =
	  static_cast<commands::connector::execute_query const*>(&(*req));
	check* new_check = new check(exec_query->get_command_id(),
				     exec_query->get_args(),
				     exec_query->get_timeout());
	connect(new_check, SIGNAL(finish(unsigned long, QString const&, int)),
		this, SLOT(_finish(unsigned long, QString const&, int)));
	_th_pool.start(new_check);
	break;
      }

      case commands::connector::request::quit_q: {
	commands::connector::quit_response quit;
	_stream << quit.build() << flush;
	QCoreApplication::instance()->quit();
	return;
      }

      default:
	break;
      }
    }
  }
  catch (std::exception const& e) {
    commands::connector::error_response error(e.what(), commands::connector::error_response::error);
    _stream << error.build() << flush;
  }
}

/**
 *  Init check if application run as root and parse argument.
 */
void dispatcher::_init() {
  if (geteuid()) {
    commands::connector::error_response error(
      "Warning: This plugin must be either run as root or setuid root.\n"
      "This plugin must be either run as root or setuid root.\n"
      "To run as root, you can use a tool like sudo.\n"
      "To set the setuid permissions, use the command:\n"
      "\tchmod u+s yourpluginfile\n",
      commands::connector::error_response::error);
    _stream << error.build() << flush;
    return;
  }

  int ret;
  unsigned int pool_size = 10;
  while ((ret = getopt(_argc, _argv, "p:")) != -1) {
    switch (ret) {
    case 'p':
      pool_size = QString(optarg).toUInt();
      break;

    default:
      commands::connector::error_response error("Bad arguments",
                                                commands::connector::error_response::warning);
      _stream << error.build() << flush;
      return;
    }
  }

  if (optind > _argc) {
    commands::connector::error_response error("Bad arguments",
                                              commands::connector::error_response::warning);
      _stream << error.build() << flush;
      return;
  }

  _th_pool.setMaxThreadCount(pool_size);
  socket_manager::instance().initialize(pool_size);

  // drop privileges.
  setuid(getuid());
}

/**
 *  Slot to notify when check finished.
 *
 *  @param[in] cmd_id    The command id.
 *  @param[in] output    The output buffer.
 *  @param[in] exit_code The exit code value.
 */
void dispatcher::_finish(unsigned long cmd_id, QString const& output, int exit_code) {
  commands::connector::execute_response execute(cmd_id,
                                                true,
                                                exit_code,
                                                QDateTime::currentDateTime(),
                                                "",
                                                output);
  _stream << execute.build() << flush;
}

/**
 *  Wait and get request.
 *
 *  @return The request was send by engine.
 */
QSharedPointer<commands::connector::request> dispatcher::_wait_request() {
  static commands::connector::request_builder& req_builder = commands::connector::request_builder::instance();
  static QList<QSharedPointer<commands::connector::request> > requests;
  static QByteArray data;
  static char buf[4096];

  while (requests.size() == 0) {
    int ret = read(0, buf, sizeof(buf) - 1);
    data.append(buf, ret);

    while (data.size() > 0) {
      int pos = data.indexOf(commands::connector::request::cmd_ending());
      if (pos < 0) {
	break;
      }

      QByteArray req_data = data.left(pos);
      data.remove(0, pos + commands::connector::request::cmd_ending().size());

      try {
	requests.push_back(req_builder.build(req_data));
      }
      catch (std::exception const& e) {
	(void)e;
      }
    }
  }

  QSharedPointer<commands::connector::request> req = requests.front();
  requests.pop_front();
  return (req);
}
