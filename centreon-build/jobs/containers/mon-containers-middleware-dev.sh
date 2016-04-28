#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Pull Centreon Web image.
docker pull ci.int.centreon.com:5000/mon-dependencies:centos$CENTOS_VERSION

# Prepare Dockerfile.
cd centreon-build/containers
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < middleware/middleware.Dockerfile.in > middleware/middleware.centos$CENTOS_VERSION.Dockerfile

# CentOS middleware image.
rm -rf centreon-imp-portal-api
cp -r ../../centreon-imp-portal-api .
docker build --no-cache -t mon-middleware-dev:centos$CENTOS_VERSION -f middleware/middleware.centos$CENTOS_VERSION.Dockerfile .
