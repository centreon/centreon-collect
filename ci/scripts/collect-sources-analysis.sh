#!/bin/bash

set -e

echo $(pwd)
echo $(ls -l)
echo $(ls -l /)
#Cmake
rm -rf /src/build
mkdir /src/build
cd /src/build/

# Variables
PROC_NBR=$( nproc )
PROJECT="centreon-collect"
MODE="$1"
AUTH_TOKEN="$2"
URL="$3"
VERSION="$4"
TARGET="$5"
if [[ "$MODE" == "PR" && -n "$6" && -n "$7" ]]; then
  PR_BRANCH="$6"
  PR_KEY="$7"
else
  echo "WARNING: Data are missing. SQ analysis will probably fail or will certainly be inconsistent ..."
fi

# Get distrib
DISTRIB=$( lsb_release -rs | cut -f1 -d. )
if [[ "$DISTRIB" = "7" ]] ; then
    source /opt/rh/devtoolset-9/enable
fi

# Prepare compilation
conan install .. -s compiler.cppstd=14 -s compiler.libcxx=libstdc++11 --build=missing
if [[ $(cat /etc/issue | awk '{print $1}') = "Debian" ]] ; then
    CXXFLAGS="-Wall -Wextra" cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DWITH_CENTREON_CLIB_INCLUDE_DIR=../clib/inc/ -DWITH_CENTREON_CLIB_LIBRARIES=centreon-clib/libcentreon_clib.so -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-engine -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-engine -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_MODULE_SIMU=On ..
else
    CXXFLAGS="-Wall -Wextra" cmake3 -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DWITH_CENTREON_CLIB_INCLUDE_DIR=../clib/inc/ -DWITH_CENTREON_CLIB_LIBRARIES=centreon-clib/libcentreon_clib.so -DCMAKE_BUILD_TYPE=Debug -DWITH_PREFIX=/usr -DWITH_PREFIX_BIN=/usr/sbin -DWITH_USER_BROKER=centreon-broker -DWITH_USER_ENGINE=centreon-engine -DWITH_GROUP_BROKER=centreon-broker -DWITH_GROUP_ENGINE=centreon-engine -DWITH_TESTING=On -DWITH_PREFIX_MODULES=/usr/share/centreon/lib/centreon-broker -DWITH_PREFIX_CONF_BROKER=/etc/centreon-broker -DWITH_PREFIX_LIB_BROKER=/usr/lib64/nagios -DWITH_PREFIX_CONF_ENGINE=/etc/centreon-engine -DWITH_PREFIX_LIB_ENGINE=/usr/lib64/centreon-engine -DWITH_PREFIX_LIB_CLIB=/usr/lib64/ -DWITH_RW_DIR=/var/lib/centreon-engine/rw -DWITH_VAR_DIR=/var/log/centreon-engine -DWITH_MODULE_SIMU=On ..
fi
cd ..

# Delete default SQ configuration file to override
rm -f /sonar-scanner/conf/sonar-scanner.properties

# Run SQ with or without reference branch
if [[ "PR" == "$MODE" ]] ; then
  # if [[ -f "/src/tmp/$PROJECT-SQ-cache-$TARGET.tar.gz" ]]; then
  #   echo "INFO: Deploying SQ cache ..."
  #   cd /src/tmp
  #   tar xzf "$PROJECT-SQ-cache-$TARGET.tar.gz"
  #   rm -rf /src/.scannerwork
  #   mv .scannerwork /src
  #   mv cache /src/build
  #   rm -rf "/src/tmp/$PROJECT-SQ-cache-$TARGET.tar.gz"
  # else
  #   echo "WARNING: Cache's tarball not found. Run a job on the reference branch to generate it."
  # fi

  echo "INFO: Running SQ in PR mode ..."
  cd /src
  /sonar-scanner/bin/sonar-scanner -X -Dsonar.scm.forceReloadAll=true -Dsonar.cfamily.threads="$PROC_NBR" -Dsonar.scm.provider=git -Dsonar.login="$AUTH_TOKEN" -Dsonar.host.url="$URL" -Dsonar.projectVersion="$VERSION" -Dsonar.pullrequest.base="$TARGET" -Dsonar.pullrequest.branch="$PR_BRANCH" -Dsonar.pullrequest.key="$PR_KEY"

  mkdir tmp
  cd /src/tmp
  
  echo "INFO: Creating cache tarball named $PROJECT-SQ-cache-$TARGET.tar.gz..."
  mv /src/.scannerwork .
  mv /src/build/cache .
  tar czf "$PROJECT-SQ-cache-$TARGET.tar.gz" cache .scannerwork
else
  # echo "INFO: Cleaning previous run files ..."
  # # if [[ -d "/src/.scannerwork" ]]; then
  # #   rm -rf /src/.scannerwork
  # # fi

  echo "INFO: Running SQ in branch mode ..."

  /sonar-scanner/bin/sonar-scanner -X -Dsonar.scm.forceReloadAll=true -Dsonar.cfamily.threads="$PROC_NBR" -Dsonar.scm.provider=git -Dsonar.login="$AUTH_TOKEN" -Dsonar.host.url="$URL" -Dsonar.projectVersion="$VERSION" -Dsonar.branch.name="$TARGET"

  echo "INFO: Cleaning tmp folder ..."
  mkdir tmp
  cd /src/tmp
  
  echo "INFO: Creating cache tarball named $PROJECT-SQ-cache-$TARGET.tar.gz..."
  mv /src/.scannerwork .
  mv /src/build/cache .
  tar czf "$PROJECT-SQ-cache-$TARGET.tar.gz" cache .scannerwork
fi
