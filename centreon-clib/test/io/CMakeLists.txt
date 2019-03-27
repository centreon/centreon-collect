##
## Copyright 2011-2013 Centreon
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## For more information : contact@centreon.com
##

# Set directories.
set(TEST_DIR "${TEST_DIR}/io")

# directory_entry tests.
#   Copy.
set(TEST_NAME "io_directory_entry_copy")
add_executable("${TEST_NAME}" "${TEST_DIR}/directory_entry/copy.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Copy.
set(TEST_NAME "io_directory_entry_current_path")
add_executable("${TEST_NAME}" "${TEST_DIR}/directory_entry/current_path.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Default constructor.
set(TEST_NAME "io_directory_entry_ctor_default")
add_executable("${TEST_NAME}" "${TEST_DIR}/directory_entry/ctor_default.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Filter.
set(TEST_NAME "io_directory_entry_filter")
add_executable("${TEST_NAME}" "${TEST_DIR}/directory_entry/filter.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# file_entry tests.
#   Copy.
set(TEST_NAME "io_file_entry_copy")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_entry/copy.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Default constructor.
set(TEST_NAME "io_file_entry_ctor_default")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_entry/ctor_default.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Pathinfo.
set(TEST_NAME "io_file_entry_pathinfo")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_entry/pathinfo.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Permission.
set(TEST_NAME "io_file_entry_permission")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_entry/permission.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Size.
set(TEST_NAME "io_file_entry_size")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_entry/size.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# file_stream tests.
#   Default constructor.
set(TEST_NAME "io_file_stream_ctor_default")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_stream/ctor_default.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Create/exists/remove.
set(TEST_NAME "io_file_stream_create_exists_remove")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_stream/create_exists_remove.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Write.
set(TEST_NAME "io_file_stream_write")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_stream/write.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Read.
set(TEST_NAME "io_file_stream_read")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_stream/read.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
#   Rename.
set(TEST_NAME "io_file_stream_rename")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_stream/rename.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
