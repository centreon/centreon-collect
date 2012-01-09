##
## Copyright 2011-2012 Merethis
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
set(TEST_DIR "${TEST_DIR}/io")

# file_stream tests.
#   Default constructor.
set(TEST_NAME "io_file_stream_ctor_default")
add_executable("${TEST_NAME}" "${TEST_DIR}/file_stream/ctor_default.cc")
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
