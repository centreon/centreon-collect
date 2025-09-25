#!/bin/bash

#
# Copyright 2025 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# For more information : contact@centreon.com
#

set -e

export UNIT_TEST=ON
export COMPILE_ONLY_AGENT=ON
export ONLY_ROBOT_CMA=ON

#lighter vcpkg than collect one


################## TO RESTORE
#mv /src/vcpkg-agent.json /src/vcpkg.json

sh /src/.github/scripts/compile-collect.sh

echo
echo "---------------------------------------- Run agent common unit tests -----------------------------------"
echo

if [ -e /usr/local/bin/gcc ]; then
  export LD_LIBRARY_PATH=/usr/local/lib64:$LD_LIBRARY_PATH
fi


cd /src/build
tests/ut_common --gtest_output=xml:ut_common.xml
tests/ut_agent --gtest_output=xml:ut_agent.xml
