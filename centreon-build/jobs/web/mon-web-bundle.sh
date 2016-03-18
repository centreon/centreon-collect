#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Pull mon-dependencies image.
docker pull ci.int.centreon.com:5000/mon-dependencies:centos$CENTOS_VERSION

# Prepare Dockerfile.
cd centreon-build/containers
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < web/web.Dockerfile.in > web/web.centos$CENTOS_VERSION.Dockerfile

# Build image.
docker build --no-cache -t ci.int.centreon.com:5000/mon-web:centos$CENTOS_VERSION -f web/web.centos$CENTOS_VERSION.Dockerfile .
docker push ci.int.centreon.com:5000/mon-web:centos$CENTOS_VERSION
