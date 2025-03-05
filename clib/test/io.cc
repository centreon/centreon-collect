/**
 * Copyright 2020 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon;

TEST(ClibIO, FileStreamCreateExistsRemove) {
  std::string path(io::file_stream::temp_path());

  // Remove old file.
  io::file_stream::remove(path.c_str());

  // File must not exists.
  ASSERT_FALSE(io::file_stream::exists(path.c_str()));

  // Create file.
  {
    io::file_stream fs;
    fs.open(path.c_str(), "w");
    fs.close();
  }

  ASSERT_TRUE(io::file_stream::exists(path.c_str()));
  ASSERT_TRUE(io::file_stream::remove(path.c_str()));
  ASSERT_FALSE(io::file_stream::exists(path.c_str()));
}

TEST(ClibIO, FileStreamCtorDefault) {
  io::file_stream fs;
  ASSERT_EQ(fs.get_native_handle(), native_handle_null);
}

TEST(ClibIO, FileStreamRead) {
  std::string tmp_file_name(io::file_stream::temp_path());

  // Open temporary file.
  io::file_stream tmp_file_stream;
  tmp_file_stream.open(tmp_file_name.c_str(), "w");

  // Return value.
  int retval(0);

  // Write.
  char const* data("some data");
  if (tmp_file_stream.write(data, static_cast<unsigned long>(strlen(data))) ==
      0)
    retval = 1;
  else {
    // NULL-read.
    try {
      tmp_file_stream.read(NULL, 1);
      retval = 1;
    } catch (const exceptions::msg_fmt& e) {
      (void)e;
    }
    // Real read.
    char buffer[1024];
    tmp_file_stream.close();
    tmp_file_stream.open(tmp_file_name.c_str(), "r");
    ASSERT_FALSE(tmp_file_stream.read(buffer, sizeof(buffer)) == 0);
  }
  ASSERT_EQ(retval, 0);
}

TEST(ClibIO, FileStreamRename) {
  // Generate temporary file name.
  char const* file_name("./rename_file_stream.test");
  char const* new_file_name("./new_rename_file_stream.test");
  char data[] = "some data";

  {
    // Open file.
    io::file_stream file(NULL, true);
    file.open(file_name, "w");

    // Write.
    ASSERT_TRUE(file.write(data, sizeof(data)));
  }

  // Rename file.
  ASSERT_TRUE(io::file_stream::rename(file_name, new_file_name));

  // Read data.
  {
    // Open file.
    io::file_stream file(NULL, true);
    file.open(new_file_name, "r");

    // Read.
    char buffer[64];
    ASSERT_EQ(file.read(buffer, sizeof(buffer)), sizeof(data));

    ASSERT_FALSE(strncmp(buffer, data, sizeof(data)));
  }

  // Remove temporary file.
  io::file_stream::remove(file_name);
  io::file_stream::remove(new_file_name);
}

TEST(ClibIO, FileStreamWrite) {
  // Generate temporary file name.
  std::string tmp_file_name(io::file_stream::temp_path());

  // Open temporary file.
  io::file_stream tmp_file_stream;
  tmp_file_stream.open(tmp_file_name.c_str(), "w");

  // NULL write.
  ASSERT_THROW(tmp_file_stream.write(NULL, 1), exceptions::msg_fmt);

  // Real write.
  char const* data("some data");
  ASSERT_FALSE(tmp_file_stream.write(
                   data, static_cast<unsigned long>(strlen(data))) == 0);
}
