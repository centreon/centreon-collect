#!/bin/bash
set -e

#Cmake
cd /src
export VCPKG_ROOT=/vcpkg
export PATH=$VCPKG_ROOT:$PATH

OS_ID=$(awk '{print $1}' /etc/issue)
if [[ "$OS_ID" == "Debian" || "$OS_ID" == "Ubuntu" ]] ; then
    CXXFLAGS="-Wall -Wextra" cmake -B build -DVCPKG_OVERLAY_TRIPLETS=/custom-triplets -DVCPKG_TARGET_TRIPLET=x64-linux-release -DVCPKG_OVERLAY_PORTS=/overlays -GNinja -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-engine -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-engine -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_MODULE_SIMU=On -S .
else
    CXXFLAGS="-Wall -Wextra" cmake -B build -GNinja -DVCPKG_OVERLAY_TRIPLETS=/custom-triplets -DVCPKG_OVERLAY_PORTS=/overlays -DVCPKG_TARGET_TRIPLET=x64-linux-release -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-engine -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-engine -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_MODULE_SIMU=On -S .
fi

#Build
ninja -Cbuild -j8
ninja -Cbuild -j8 install

#Test

cd build
tests/ut_broker --gtest_output=xml:ut_broker.xml
tests/ut_engine --gtest_output=xml:ut_engine.xml
tests/ut_clib --gtest_output=xml:ut_clib.xml
tests/ut_connector --gtest_output=xml:ut_connector.xml
tests/ut_common --gtest_output=xml:ut_common.xml
echo "----------------------------------------------------------   end of ut tests ------------------------------------------------"
