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
set(TEST_DIR "${TEST_DIR}/exceptions")

# basic_constructor
set(TEST_NAME "basic_constructor")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# basic_copy
set(TEST_NAME "basic_copy")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# basic_insert_char
set(TEST_NAME "basic_insert_char")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# basic_insert_long
set(TEST_NAME "basic_insert_long")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# basic_insert_pchar
set(TEST_NAME "basic_insert_pchar")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# basic_insert_ulong
set(TEST_NAME "basic_insert_ulong")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# basic_insert_int
set(TEST_NAME "basic_insert_int")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# basic_insert_longlong
set(TEST_NAME "basic_insert_longlong")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# basic_insert_uint
set(TEST_NAME "basic_insert_uint")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# basic_insert_ulonglong
set(TEST_NAME "basic_insert_ulonglong")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
