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

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <QDir>
#include <QFile>
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/engine.hh"
#include "com/centreon/engine/logging/file.hh"
#include "com/centreon/engine/logging/object.hh"
#include "com/centreon/shared_ptr.hh"
#include "test/unittest.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

/**
 *  Check the file's content.
 *
 *  @param[in] filename The file name.
 *  @param[in] text     The content reference.
 */
static void check_file(
              std::string const& filename,
              std::string const& text) {
  QFile file(filename.c_str());
  file.open(QIODevice::ReadOnly);
  if (file.error() != QFile::NoError)
    throw (engine_error() << filename << ": "
           << file.errorString().toStdString());

  if (file.readAll() != text.c_str())
    throw (engine_error() << filename << ": bad content");

  file.close();
}

/**
 *  Check the logging file working.
 *  - file rotate.
 *  - file truncate.
 */
int main_test(int argc, char** argv) {
  (void)argc;
  (void)argv;

  // Get instance of logging engine.
  engine& engine = engine::instance();
  unsigned int id1 = 0;
  unsigned int id2 = 0;
  unsigned int id3 = 0;

  {
    // Add new object (file) to log into engine and test limit size.
    com::centreon::shared_ptr<object>
      obj1(new file("./test_logging_file_size_limit.log", 10));
    engine::obj_info info1(obj1, log_all, most);
    id1 = engine.add_object(info1);

    // Add new object (file) to log into engine and test no limit size.
    com::centreon::shared_ptr<object>
      obj2(new file("./test_logging_file.log"));
    engine::obj_info info2(obj2, log_all, most);
    id2 = engine.add_object(info2);

    // Add new object (file) to log into engine and test reopen.
    com::centreon::shared_ptr<object>
      obj3(new file("./test_logging_file_reopen.log"));
    engine::obj_info info3(obj3, log_all, most);
    id3 = engine.add_object(info3);
  }

  // Send message to all object.
  engine.log("012345", log_info_message, basic);
  engine.log("0123456789", log_info_message, basic);
  // Reopen files.
  if (rename("./test_logging_file_reopen.log", "./test_logging_file_reopen.log.old"))
    throw (engine_error() << "rename failed: " << strerror(errno));
  file::reopen();
  // Send message to all object.
  engine.log("qwerty", log_info_message, basic);

  // Cleanup.
  engine.remove_object(id3);
  engine.remove_object(id2);
  engine.remove_object(id1);

  QDir dir("./");
  QStringList filters("test_logging_file*.log*");
  QFileInfoList files = dir.entryInfoList(filters);

  // Check the content of all file log.
  for (QFileInfoList::const_iterator it = files.begin(), end = files.end();
       it != end;
       ++it) {
    if (it->fileName() == "test_logging_file_size_limit.log")
      check_file(it->fileName().toStdString(), "qwerty");
    else if (it->fileName() == "test_logging_file_size_limit.log.old")
      check_file(it->fileName().toStdString(), "0123456789");
    else if (it->fileName() == "test_logging_file.log")
      check_file(
        it->fileName().toStdString(),
        "0123450123456789qwerty");
    else if (it->fileName() == "test_logging_file_reopen.log")
      check_file(
        it->fileName().toStdString(),
        "qwerty");
    else if (it->fileName() == "test_logging_file_reopen.log.old")
      check_file(it->fileName().toStdString(), "0123450123456789");
    else
      throw (engine_error() << "bad file name");
    remove(it->fileName().toStdString().c_str());
  }

  return (0);
}

/**
 *  Init unit test.
 */
int main(int argc, char** argv) {
  unittest utest(argc, argv, &main_test);
  return (utest.run());
}
