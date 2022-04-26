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
cd ..

# Get thread number
PROCNBR=$( nproc )
VERSION="$4"
PROJECT="centreon-collect"
SET_CACHE=0
# Delete default configuration file to override
rm -f /src/tmp/sonar-scanner/conf/sonar-scanner.properties

# Run SQ with or without reference branch
if [[ "PR" == "$1" ]] ; then
  if [[ -f "/src/tmp/$PROJECT-SQ-cache-$VERSION.tar.gz" ]]; then
    echo "INFO: Deploying SQ cache ..."
    cd /src/tmp
    tar xzf "$PROJECT-SQ-cache-$VERSION.tar.gz"
    mv .scannerwork /src
    mv cache /src/build
  else
    echo "INFO: Cache's tarball not found. The cache will be recomputed after the analysis ..."
    SET_CACHE=1
  fi

  echo "INFO: Running SQ in PR mode ..."
  cd /src
  /src/tmp/sonar-scanner/bin/sonar-scanner -X -Dsonar.scm.forceReloadAll=true -Dsonar.cfamily.threads="$PROCNBR" -Dsonar.scm.provider=git -Dsonar.login="$2" -Dsonar.host.url="$3" -Dsonar.projectVersion="$VERSION" -Dsonar.pullrequest.branch="$5" -Dsonar.pullrequest.base="$6" -Dsonar.pullrequest.key="$7"
else
  echo "INFO: Cleaning previous run files ..."
  if [[ -d "/src/.scannerwork" ]]; then
    rm -rf /src/.scannerwork
  fi

  echo "INFO: Running SQ in branch mode ..."
  /src/tmp/sonar-scanner/bin/sonar-scanner -X -Dsonar.scm.forceReloadAll=true -Dsonar.cfamily.threads="$PROCNBR" -Dsonar.scm.provider=git -Dsonar.login="$2" -Dsonar.host.url="$3" -Dsonar.projectVersion="$VERSION" -Dsonar.branch.name="$5"

  # Ask for cache saving
  SET_CACHE=1
fi

# Create cache's tarball if required
if [[ 1 -eq "$SET_CACHE" ]]; then
  echo "INFO: Cleaning tmp folder ..."
  cd /src/tmp
  rm -f "$PROJECT-SQ-cache-$VERSION.tar.gz"
  echo "INFO: Moving cache as tarball ..."
  mv /src/.scannerwork .
  mv /src/build/cache .
  tar czf "$PROJECT-SQ-cache-$VERSION.tar.gz" cache .scannerwork
fi