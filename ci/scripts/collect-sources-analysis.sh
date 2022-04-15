#!/bin/bash
set -e

#Cmake
rm -rf /src/build
mkdir /src/build
cd /src/build/

DISTRIB=$( lsb_release -rs | cut -f1 -d. )
if [ "$DISTRIB" = "7" ] ; then
    source /opt/rh/devtoolset-9/enable
fi
conan install .. -s compiler.cppstd=14 -s compiler.libcxx=libstdc++11 --build=missing
if [ $(cat /etc/issue | awk '{print $1}') = "Debian" ] ; then
    CXXFLAGS="-Wall -Wextra" cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DWITH_CENTREON_CLIB_INCLUDE_DIR=../clib/inc/ -DWITH_CENTREON_CLIB_LIBRARIES=centreon-clib/libcentreon_clib.so -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-engine -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-engine -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_MODULE_SIMU=On ..
else
    CXXFLAGS="-Wall -Wextra" cmake3 -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DWITH_CENTREON_CLIB_INCLUDE_DIR=../clib/inc/ -DWITH_CENTREON_CLIB_LIBRARIES=centreon-clib/libcentreon_clib.so -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-engine -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-engine -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_MODULE_SIMU=On ..
fi

cd ..

echo "host url = $3"
echo "is PR ? = $1"

if [[ -n "$4" ]]; then
  echo "var4 = $4"
fi

if [[ -n "$5" ]]; then
  echo "var5 = $5"
fi

if [[ -n "$6" ]]; then
  echo "var6 = $6"
fi

# Get thread number
PROCNBR=$( nproc )

if [ "PR" == "$1" ] ; then
  SONAR=$( /src/tmp/sonar-scanner/bin/sonar-scanner -X -Dsonar.cfamily.threads="$PROCNBR" -Dsonar.projectVersion="$VERSION" -Dsonar.login="$2" -Dsonar.host.url="$3" -Dsonar.pullrequest.branch="$4" -Dsonar.pullrequest.base="$5" -Dsonar.pullrequest.key="$6" )
else
  SONAR=$( /src/tmp/sonar-scanner/bin/sonar-scanner -X -Dsonar.cfamily.threads="$PROCNBR" -Dsonar.projectVersion="$VERSION" -Dsonar.login="$2" -Dsonar.host.url="$3" -Dsonar.pullrequest.branch="$4" )
fi
