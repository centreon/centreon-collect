#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Prepare Dockerfiles.
cd centreon-build/containers
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < web/stable.Dockerfile.in > web/stable.centos$CENTOS_VERSION.Dockerfile

# Build 'stable' image.
docker build --no-cache -t ci.int.centreon.com:5000/mon-web-stable:centos$CENTOS_VERSION -f web/stable.centos$CENTOS_VERSION.Dockerfile .
docker push ci.int.centreon.com:5000/mon-web-stable:centos$CENTOS_VERSION
