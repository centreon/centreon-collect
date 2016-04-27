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
docker pull ci.int.centreon.com:5000/mon-lm:centos$CENTOS_VERSION

# Prepare Dockerfile.
cd centreon-build/containers
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < ppm/ppm-dev.Dockerfile.in > ppm/ppm-dev.centos$CENTOS_VERSION.Dockerfile

# CentOS PPM image.
rm -rf centreon-pp-manager
cp -r ../../centreon-import/www/modules/centreon-pp-manager .
docker build --no-cache -t mon-ppm-dev:centos$CENTOS_VERSION -f ppm/ppm-dev.centos$CENTOS_VERSION.Dockerfile .

