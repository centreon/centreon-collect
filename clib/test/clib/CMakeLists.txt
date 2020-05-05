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
