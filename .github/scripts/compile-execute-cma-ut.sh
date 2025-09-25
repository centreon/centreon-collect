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
set -x

echo "---------------------------------------- Install sccache ------------------------------------------------"

export SCCACHE_PATH="/usr/bin/sccache"

wget https://github.com/mozilla/sccache/releases/download/v0.9.1/sccache-v0.9.1-x86_64-unknown-linux-musl.tar.gz
tar xzf sccache-v0.9.1-x86_64-unknown-linux-musl.tar.gz
mv sccache-v0.9.1-x86_64-unknown-linux-musl/sccache /usr/bin/

${SCCACHE_PATH} --start-server


echo "-------------------------------------- Compile ut_agent and ut_common -----------------------------------"

CMAKE="cmake"
if [ -f /usr/bin/cmake3 ]; then
  CMAKE="cmake3"
fi

cd /src

#lighter vcpkg than collect one
mv vcpkg-agent.json vcpkg.json

export VCPKG_ROOT=/vcpkg
export PATH=$VCPKG_ROOT:$PATH

#in case of centos, we had compiled 9.5 version of gcc with /usr/local as prefix
if [ -e /usr/local/bin/gcc ]; then
  export CC=/usr/local/bin/gcc
  export CXX=/usr/local/bin/g++
  export LD_LIBRARY_PATH=/usr/local/lib64:$LD_LIBRARY_PATH
fi

cd /src

CXXFLAGS="-Wall -Wextra" $CMAKE \
        -B build \
        -DVCPKG_OVERLAY_TRIPLETS=/custom-triplets \
        -DVCPKG_TARGET_TRIPLET=x64-linux-release \
        -DVCPKG_OVERLAY_PORTS=/overlays \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DWITH_PREFIX=/usr \
        -DWITH_TESTING=On \
        -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ \
        -DCMAKE_C_COMPILER_LAUNCHER=${SCCACHE_PATH} \
        -DCMAKE_CXX_COMPILER_LAUNCHER=${SCCACHE_PATH} \
        -DLEGACY_ENGINE=Off \
        -DCOMPILE_ONLY_AGENT=ON \
        -S .

ninja -Cbuild
ninja -Cbuild install

echo "---------------------------------------- sccache statistics --------------------------------------------"
${SCCACHE_PATH} --show-stats

echo "---------------------------------------- Stop sccache --------------------------------------------------"
${SCCACHE_PATH} --stop-server

echo "---------------------------------------- Run agent common unit tests -----------------------------------"
cd /src/build
tests/ut_common --gtest_output=xml:ut_common.xml
tests/ut_agent --gtest_output=xml:ut_agent.xml
