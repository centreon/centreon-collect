#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 2 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Pull mon-dependencies image.
docker pull ci.int.centreon.com:5000/mon-dependencies:centos$CENTOS_VERSION

# Prepare Dockerfiles.
cd centreon-build/containers
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < web/fresh.Dockerfile.in > web/fresh.centos$CENTOS_VERSION.Dockerfile
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < web/standard.Dockerfile.in > web/standard.centos$CENTOS_VERSION.Dockerfile

# Build 'fresh' image.
docker build --no-cache -t ci.int.centreon.com:5000/mon-web-fresh:centos$CENTOS_VERSION -f web/fresh.centos$CENTOS_VERSION.Dockerfile .
docker push ci.int.centreon.com:5000/mon-web-fresh:centos$CENTOS_VERSION

# Build 'standard' image.
docker build --no-cache -t ci.int.centreon.com:5000/mon-web:centos$CENTOS_VERSION -f web/standard.centos$CENTOS_VERSION.Dockerfile .
docker push ci.int.centreon.com:5000/mon-web:centos$CENTOS_VERSION
