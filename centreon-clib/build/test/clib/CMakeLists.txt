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

# Set test directory.
set(TEST_DIR "${TEST_DIR}/clib")

# Version major.
set(TEST_NAME "clib_version_major")
add_executable("${TEST_NAME}" "${TEST_DIR}/version/major.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
# Version minor.
set(TEST_NAME "clib_version_minor")
add_executable("${TEST_NAME}" "${TEST_DIR}/version/minor.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
# Version patch.
set(TEST_NAME "clib_version_patch")
add_executable("${TEST_NAME}" "${TEST_DIR}/version/patch.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
# Version string.
set(TEST_NAME "clib_version_string")
add_executable("${TEST_NAME}" "${TEST_DIR}/version/string.cc")
target_link_libraries("${TEST_NAME}" "${DEFAULT_LINK_NAME}")
add_test("${TEST_NAME}" "${TEST_NAME}")
