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
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < lm/lm-dev.Dockerfile.in > lm/lm-dev.centos$CENTOS_VERSION.Dockerfile

# CentOS PPM image.
rm -rf centreon-license-manager
cd ../../centreon-license-manager/www/modules/centreon-license-manager/frontend/app
gulp
cd ../../../../../../centreon-build/containers
cp -r ../../centreon-license-manager/www/modules/centreon-license-manager .
docker build -t mon-lm-dev:centos$CENTOS_VERSION -f lm/lm-dev.centos$CENTOS_VERSION.Dockerfile .

