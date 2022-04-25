#!/bin/bash
set -e

#Cmake
rm -rf /src/build
mkdir /src/build
cd /src/build/

DISTRIB=$( lsb_release -rs | cut -f1 -d. )
if [[ "$DISTRIB" = "7" ]] ; then
    source /opt/rh/devtoolset-9/enable
fi
conan install .. -s compiler.cppstd=14 -s compiler.libcxx=libstdc++11 --build=missing
if [[ $(cat /etc/issue | awk '{print $1}') = "Debian" ]] ; then
    CXXFLAGS="-Wall -Wextra" cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DWITH_CENTREON_CLIB_INCLUDE_DIR=../clib/inc/ -DWITH_CENTREON_CLIB_LIBRARIES=centreon-clib/libcentreon_clib.so -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-engine -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-engine -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_MODULE_SIMU=On ..
else
    CXXFLAGS="-Wall -Wextra" cmake3 -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DWITH_CENTREON_CLIB_INCLUDE_DIR=../clib/inc/ -DWITH_CENTREON_CLIB_LIBRARIES=centreon-clib/libcentreon_clib.so -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-engine -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-engine -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_MODULE_SIMU=On ..
fi
#cd ..

# Get thread number
PROCNBR=$( nproc )
VERSION="$4"
PROJECT="centreon-collect"
# Delete default configuration file to override
rm -f /src/tmp/sonar-scanner/conf/sonar-scanner.properties

# Run SQ with or without reference branch
#if [[ "PR" == "$1" ]] ; then

  if [[ -f "$PROJECT-SQ-cache-$VERSION.tar.gz" ]]; then
    echo "INFO: Deploying SQ cache ..."
    tar xzf "$PROJECT-SQ-cache-$VERSION.tar.gz"

    mv /src/build/.scannerwork /src
  else
    echo "INFO: Cache's tarball not found. The cache will be recomputed..."
  fi

echo "build state"
pwd
ls -la

  echo "INFO: Running SQ in PR mode ..."
  cd /src
  /src/tmp/sonar-scanner/bin/sonar-scanner -X -Dsonar.scm.forceReloadAll=true -Dsonar.cfamily.threads="$PROCNBR" -Dsonar.scm.provider=git -Dsonar.login="$2" -Dsonar.host.url="$3" -Dsonar.projectVersion="$VERSION" -Dsonar.pullrequest.branch="$5" -Dsonar.pullrequest.base="$6" -Dsonar.pullrequest.key="$7"
#else
#  echo "INFO: Cleaning cached files ..."
#  if [[ -d "/src/.scannerwork" ]]; then
#    rm -rf /src/.scannerwork
#  fi

#  echo "INFO: Running SQ in branch mode ..."
#  /src/tmp/sonar-scanner/bin/sonar-scanner -X -Dsonar.scm.forceReloadAll=true -Dsonar.cfamily.threads="$PROCNBR" -Dsonar.scm.provider=git -Dsonar.login="$2" -Dsonar.host.url="$3" -Dsonar.projectVersion="$VERSION" -Dsonar.branch.name="$5"

  echo "INFO: Moving cache as tarball ..."
  mv /src/.scannerwork /src/build
  cd /src/build
  tar czf "$PROJECT-SQ-cache-$VERSION.tar.gz" cache .scannerwork
#fi
