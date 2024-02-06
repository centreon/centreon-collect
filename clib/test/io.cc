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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/directory_entry.hh"
#include "com/centreon/io/file_stream.hh"

unsigned int const DATA_SIZE = 42;

using namespace com::centreon;

static void create_fake_file(std::string const& path) {
  if (!io::file_stream::exists(path)) {
    io::file_stream fs;
    fs.open(path, "w");
    fs.close();
  }
}

TEST(ClibIO, DirEntryCopy) {
  io::directory_entry e1(".");
  io::directory_entry e2(e1);
  ASSERT_EQ(e1, e2);

  io::directory_entry e3 = e1;
  ASSERT_EQ(e1, e3);
}

TEST(ClibIO, DirCtorDefault) {
  io::directory_entry entry(NULL);
  ASSERT_TRUE(entry.entry().path().empty());
  io::directory_entry entry2(".");
  ASSERT_FALSE(entry2.entry().path().empty());
}

TEST(ClibIO, DirCurrentPath) {
  std::string path(io::directory_entry::current_path());
  ASSERT_FALSE(path.empty());
}

TEST(ClibIO, DirFilter) {
  io::directory_entry entry(".");
  std::list<io::file_entry> lst_all(entry.entry_list());
  std::list<io::file_entry> lst_point(entry.entry_list(".*"));
  std::list<io::file_entry> lst_de(entry.entry_list("io_directory_entry*"));

  ASSERT_GE(lst_all.size(), lst_point.size() + lst_de.size());
  ASSERT_GE(lst_point.size(), 2u);
  ASSERT_LT(lst_de.size(), 2u);
}

TEST(ClibIO, FileEntryCopy) {
  io::file_entry e1("/tmp");
  io::file_entry e2(e1);
  ASSERT_EQ(e1, e2);

  io::file_entry e3 = e1;
  ASSERT_EQ(e1, e3);
}

TEST(ClibIO, FileEntryCtorDefault) {
  {
    io::file_entry entry(NULL);
    ASSERT_TRUE(entry.path().empty());
  }

  {
    io::file_entry entry("/tmp");
    ASSERT_FALSE(entry.path().empty());
  }
}

TEST(ClibIO, FileEntryPathInfo) {
  std::string p1("/tmp/test.ext");
  std::string p2("/tmp/.test");
  std::string p3("/tmp/test");

  create_fake_file(p1);
  create_fake_file(p2);
  create_fake_file(p3);

  io::file_entry e1(p1);
  ASSERT_EQ(e1.base_name(), "test");
  ASSERT_EQ(e1.file_name(), "test.ext");
  ASSERT_EQ(e1.directory_name(), "/tmp");

  io::file_entry e2(p2);
  ASSERT_EQ(e2.base_name(), ".test");
  ASSERT_EQ(e2.file_name(), ".test");
  ASSERT_EQ(e2.directory_name(), "/tmp");

  io::file_entry e3(p3);
  ASSERT_EQ(e3.base_name(), "test");
  ASSERT_EQ(e3.file_name(), "test");
  ASSERT_EQ(e3.directory_name(), "/tmp");

  io::file_stream::remove(p1);
  io::file_stream::remove(p2);
  io::file_stream::remove(p3);
}

TEST(ClibIO, FileEntryPermission) {
  io::file_entry entry("/tmp");
  ASSERT_TRUE(entry.is_directory());
  ASSERT_FALSE(entry.is_link());
  ASSERT_FALSE(entry.is_regular());
}

TEST(ClibIO, FileEntrySize) {
  std::string temp(io::file_stream::temp_path());
  {
    io::file_stream fs;
    fs.open(temp, "w");
    fs.close();
  }

  io::file_entry entry(temp);
  ASSERT_FALSE(entry.size());

  {
    std::string data(DATA_SIZE, ' ');
    io::file_stream fs;
    fs.open(temp, "w");
    fs.write(data.c_str(), data.size());
    fs.close();
  }

  ASSERT_FALSE(entry.size());
  entry.refresh();
  ASSERT_EQ(entry.size(), DATA_SIZE);

  if (io::file_stream::exists(temp))
    io::file_stream::remove(temp);
}

TEST(ClibIO, FileStreamCreateExistsRemove) {
  char* path(io::file_stream::temp_path());

  // Remove old file.
  io::file_stream::remove(path);

  // File must not exists.
  ASSERT_FALSE(io::file_stream::exists(path));

  // Create file.
  {
    io::file_stream fs;
    fs.open(path, "w");
    fs.close();
  }

  ASSERT_TRUE(io::file_stream::exists(path));
  ASSERT_TRUE(io::file_stream::remove(path));
  ASSERT_FALSE(io::file_stream::exists(path));
}

TEST(ClibIO, FileStreamCtorDefault) {
  io::file_stream fs;
  ASSERT_EQ(fs.get_native_handle(), native_handle_null);
}

TEST(ClibIO, FileStreamRead) {
  char const* tmp_file_name(io::file_stream::temp_path());

  // Open temporary file.
  io::file_stream tmp_file_stream;
  tmp_file_stream.open(tmp_file_name, "w");

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
    } catch (exceptions::basic const& e) {
      (void)e;
    }
    // Real read.
    char buffer[1024];
    tmp_file_stream.close();
    tmp_file_stream.open(tmp_file_name, "r");
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
  char const* tmp_file_name(io::file_stream::temp_path());

  // Open temporary file.
  io::file_stream tmp_file_stream;
  tmp_file_stream.open(tmp_file_name, "w");

  // NULL write.
  ASSERT_THROW(tmp_file_stream.write(NULL, 1), exceptions::basic);

  // Real write.
  char const* data("some data");
  ASSERT_FALSE(tmp_file_stream.write(
                   data, static_cast<unsigned long>(strlen(data))) == 0);
}
