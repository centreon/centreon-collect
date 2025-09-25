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

echo
echo "---------------------------------------- Install sccache ------------------------------------------------"
echo

export SCCACHE_PATH="/usr/bin/sccache"

if [ "${ARCH}" = "amd64" ]; then
    wget https://github.com/mozilla/sccache/releases/download/v0.9.1/sccache-v0.9.1-x86_64-unknown-linux-musl.tar.gz
    tar xzf sccache-v0.9.1-x86_64-unknown-linux-musl.tar.gz
    mv sccache-v0.9.1-x86_64-unknown-linux-musl/sccache /usr/bin/
elif [ "${ARCH}" = "arm64" ]; then
    wget https://github.com/mozilla/sccache/releases/download/v0.9.1/sccache-v0.9.1-aarch64-unknown-linux-musl.tar.gz
    tar xzf sccache-v0.9.1-aarch64-unknown-linux-musl.tar.gz
    mv sccache-v0.9.1-aarch64-unknown-linux-musl/sccache /usr/bin/
fi
${SCCACHE_PATH} --start-server

echo
echo "-------------------------------------- Compile -----------------------------------"
echo

CMAKE="cmake"
if [ -f /usr/bin/cmake3 ]; then
  CMAKE="cmake3"
fi

if [ "${ARCH}" = "arm64" ]; then
    export VCPKG_FORCE_SYSTEM_BINARIES=1
    export TRIPLET=arm64-linux-release
else
    export TRIPLET=x64-linux-release
fi

export VCPKG_ROOT=/vcpkg
export PATH=$VCPKG_ROOT:$PATH

#in case of centos, we had compiled 9.5 version of gcc with /usr/local as prefix
if [ -e /usr/local/bin/gcc ]; then
  export CC=/usr/local/bin/gcc
  export CXX=/usr/local/bin/g++
  export LD_LIBRARY_PATH=/usr/local/lib64:$LD_LIBRARY_PATH
fi

cd /src

${CMAKE} \
        -B build \
        -DVCPKG_OVERLAY_TRIPLETS=/custom-triplets \
        -DVCPKG_TARGET_TRIPLET=$TRIPLET \
        -DVCPKG_OVERLAY_PORTS=/overlays \
        -GNinja \
        -DDEBUG_ROBOT=OFF \
        -DWITH_TESTING=$UNIT_TEST \
        -DWITH_BENCH=ON \
        -DWITH_MODULE_SIMU=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DWITH_STARTUP_SCRIPT=systemd \
        -DWITH_ENGINE_LOGROTATE_SCRIPT=ON \
        -DWITH_USER_BROKER=centreon-broker \
        -DWITH_GROUP_BROKER=centreon-broker \
        -DWITH_USER_ENGINE=centreon-engine \
        -DWITH_GROUP_ENGINE=centreon-engine \
        -DWITH_VAR_DIR=/var/log/centreon-engine \
        -DWITH_DAEMONS=ON \
        -DWITH_CREATE_FILES=OFF \
        -DWITH_CONFIG_FILES=ON \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_COMPILER_LAUNCHER=${SCCACHE_PATH} \
        -DCMAKE_CXX_COMPILER_LAUNCHER=${SCCACHE_PATH} \
        -DLEGACY_ENGINE=${LEGACY_ENGINE} \
        -DONLY_ROBOT_CMA=${ONLY_ROBOT_CMA} \
        -DCOMPILE_ONLY_AGENT=${COMPILE_ONLY_AGENT} \
        -S .

ninja -Cbuild

echo
echo "---------------------------------------- sccache statistics --------------------------------------------"
${SCCACHE_PATH} --show-stats
echo

echo
echo "---------------------------------------- Stop sccache --------------------------------------------------"
${SCCACHE_PATH} --stop-server
echo