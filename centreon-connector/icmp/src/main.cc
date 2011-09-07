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

#include <QCoreApplication>
#include <QDebug>
#include <exception>
#include "com/centreon/connector/icmp/dispatcher.hh"
#include "com/centreon/connector/icmp/engine.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Check host is alive by icmp check.
 */
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  QString progname(argv[0]);
  progname = progname.right(progname.size() - progname.lastIndexOf('/') - 1);
  app.setApplicationName(progname);

  dispatcher dispatch(argc, argv);
  dispatch.start();

  app.exec();
  return (STATE_OK);
}
