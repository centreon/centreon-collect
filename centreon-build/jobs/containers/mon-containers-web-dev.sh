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
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < web/dev.Dockerfile.in > web/dev.centos$CENTOS_VERSION.Dockerfile

# Build 'dev' image.
rm -rf centreon
cp -r ../../centreon .
docker build --no-cache -t ci.int.centreon.com:5000/mon-web-dev:centos$CENTOS_VERSION -f web/dev.centos$CENTOS_VERSION.Dockerfile .
