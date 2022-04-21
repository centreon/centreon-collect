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
PROJECT="centreon-collect"

# Run SQ with or without reference branch
#if [[ "PR" == "$1" ]] ; then
  echo "Getting SQ cache"
  if [[ -f "/src/build/$PROJECT-SQ-cache-$VERSION.tar.gz" ]]; then
    cd /src/build
    tar xzf "$PROJECT-SQ-cache-$VERSION.tar.gz"
    mv /src/build/cache /root/.sonar
    cd ..
  else
    echo "WARNING: Cache's tarball not found. The cache will be recomputed..."
  fi

  echo "Running SQ in PR mode"
  /src/tmp/sonar-scanner/bin/sonar-scanner -X -Dsonar.scm.forceReloadAll=true -Dsonar.cfamily.threads="$PROCNBR" -Dsonar.scm.provider=git -Dsonar.login="$2" -Dsonar.host.url="$3" -Dsonar.projectVersion="$4" -Dsonar.pullrequest.branch="$5" -Dsonar.pullrequest.base="$6" -Dsonar.pullrequest.key="$7"
#else
#  echo "Cleaning previous cache"
#  if [[ -d "/src/build/cache" ]]; then
#    rm -rf /src/build/cache
#  fi
#  if [[ -d "/root/.sonar/cache" ]]; then
#    rm -rf /root/.sonar/cache
#  fi

#  echo "Running SQ in branch mode"
#  /src/tmp/sonar-scanner/bin/sonar-scanner -X -Dsonar.scm.forceReloadAll=true -Dsonar.cfamily.threads="$PROCNBR" -Dsonar.scm.provider=git -Dsonar.login="$2" -Dsonar.host.url="$3" -Dsonar.projectVersion="$4" -Dsonar.branch.name="$5"

  echo "Saving new cache"
  mv /root/.sonar/cache /src/build
  cd /src/build
  tar czf "$PROJECT-$VERSION-SQ-source.tar.gz" cache
fi
