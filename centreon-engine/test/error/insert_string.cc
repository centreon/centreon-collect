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

#include <QCoreApplication>
#include <string>
#include <string.h>
#include "com/centreon/engine/error.hh"

using namespace com::centreon::engine;

/**
 *  Check that std::string insertion works with error.
 *
 *  @return 0 on success.
 */
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  // Strings.
  std::string s1("foo");
  std::string s2(" bar baz");
  std::string s3(" qux");

  // Insert strings.
  error e;
  e << s1 << s2;
  e << s3;

  // Check.
  return (strcmp("foo bar baz qux", e.what()));
}
