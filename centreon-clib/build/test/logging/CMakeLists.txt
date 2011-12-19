##
## Copyright 2011 Merethis
##
## This file is part of Centreon Clib.
##
## Centreon Clib is free software: you can redistribute it
## and/or modify it under the terms of the GNU Affero General Public
## License as published by the Free Software Foundation, either version
## 3 of the License, or (at your option) any later version.
##
## Centreon Clib is distributed in the hope that it will be
## useful, but WITHOUT ANY WARRANTY; without even the implied warranty
## of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## Affero General Public License for more details.
##
## You should have received a copy of the GNU Affero General Public
## License along with Centreon Clib. If not, see
## <http://www.gnu.org/licenses/>.
##

# Set directories.
set(TEST_DIR "${TEST_DIR}/logging")

# engine_add
set(TEST_NAME "engine_add")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_disable_sync
set(TEST_NAME "engine_disable_sync")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_enable_sync
set(TEST_NAME "engine_enable_sync")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_is_log
set(TEST_NAME "engine_is_log")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_log
set(TEST_NAME "engine_log")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_log_multiline
set(TEST_NAME "engine_log_multiline")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_remove_by_backend
set(TEST_NAME "engine_remove_by_backend")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_remove_by_id
set(TEST_NAME "engine_remove_by_id")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_with_pid
set(TEST_NAME "engine_with_pid")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_with_timestamp
set(TEST_NAME "engine_with_timestamp")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_with_thread_id
set(TEST_NAME "engine_with_thread_id")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# file_log
set(TEST_NAME "file_log")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# logger_copy
set(TEST_NAME "logger_copy")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# engine_with_thread
set(TEST_NAME "engine_with_thread")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_constructor
set(TEST_NAME "temp_logger_constructor")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_char
set(TEST_NAME "temp_logger_do_nothing_char")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_double
set(TEST_NAME "temp_logger_do_nothing_double")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_int
set(TEST_NAME "temp_logger_do_nothing_int")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_long
set(TEST_NAME "temp_logger_do_nothing_long")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_long_long
set(TEST_NAME "temp_logger_do_nothing_long_long")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_pvoid
set(TEST_NAME "temp_logger_do_nothing_pvoid")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_std_string
set(TEST_NAME "temp_logger_do_nothing_std_string")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_string
set(TEST_NAME "temp_logger_do_nothing_string")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_uint
set(TEST_NAME "temp_logger_do_nothing_uint")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_ulong
set(TEST_NAME "temp_logger_do_nothing_ulong")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_do_nothing_ulong_long
set(TEST_NAME "temp_logger_do_nothing_ulong_long")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_char
set(TEST_NAME "temp_logger_log_char")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_double
set(TEST_NAME "temp_logger_log_double")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_int
set(TEST_NAME "temp_logger_log_int")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_long
set(TEST_NAME "temp_logger_log_long")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_long_long
set(TEST_NAME "temp_logger_log_long_long")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_pvoid
set(TEST_NAME "temp_logger_log_pvoid")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_std_string
set(TEST_NAME "temp_logger_log_std_string")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_string
set(TEST_NAME "temp_logger_log_string")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_uint
set(TEST_NAME "temp_logger_log_uint")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_ulong
set(TEST_NAME "temp_logger_log_ulong")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# temp_logger_log_ulong_long
set(TEST_NAME "temp_logger_log_ulong_long")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
