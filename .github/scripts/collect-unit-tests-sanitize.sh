#!/bin/bash
set -e

dnf install llvm libasan

#Cmake
rm -rf /src/build
mkdir /src/build
cd /src/build/

conan install .. -s compiler.cppstd=14 -s compiler.libcxx=libstdc++11 --build=missing
if [ $(cat /etc/issue | awk '{print $1}') = "Debian" ] ; then
    CXXFLAGS="-Wall -Wextra" cmake -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DWITH_ASAN=ON -DWITH_CENTREON_CLIB_INCLUDE_DIR=../clib/inc/ -DWITH_CENTREON_CLIB_LIBRARIES=centreon-clib/libcentreon_clib.so -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-clib -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-clib -DWITH_SQL_TESTS=ON -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-clib -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-clib -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-clib/rw -DWITH_VAR_DIR=/var/log/centreon-clib -DWITH_MODULE_SIMU=On ..
else
    CXXFLAGS="-Wall -Wextra" cmake3 -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DWITH_ASAN=ON -DWITH_CENTREON_CLIB_INCLUDE_DIR=../clib/inc/ -DWITH_CENTREON_CLIB_LIBRARIES=centreon-clib/libcentreon_clib.so -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-clib -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-clib -DWITH_SQL_TESTS=ON -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-clib -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-clib -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-clib/rw -DWITH_VAR_DIR=/var/log/centreon-clib -DWITH_MODULE_SIMU=On ..
fi

#Build
ninja -j 8

#Test

ASAN_OPTIONS="symbolize=1 detect_odr_violation=0" ASAN_SYMBOLIZER_PATH=$(which llvm-symbolizer) tests/ut_broker --gtest_output=xml:ut_broker.xml
ASAN_OPTIONS="symbolize=1 detect_odr_violation=0" ASAN_SYMBOLIZER_PATH=$(which llvm-symbolizer) tests/ut_clib --gtest_output=xml:ut_clib.xml
ASAN_OPTIONS="symbolize=1 detect_odr_violation=0" ASAN_SYMBOLIZER_PATH=$(which llvm-symbolizer) tests/ut_clib --gtest_output=xml:ut_clib.xml
ASAN_OPTIONS="symbolize=1 detect_odr_violation=0" ASAN_SYMBOLIZER_PATH=$(which llvm-symbolizer) tests/ut_connector --gtest_output=xml:ut_connector.xml
echo "----------------------------------------------------------   end of ut tests ------------------------------------------------"
