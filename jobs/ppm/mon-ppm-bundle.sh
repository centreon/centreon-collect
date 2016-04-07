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
docker pull ci.int.centreon.com:5000/mon-web:centos$CENTOS_VERSION

# Fetch PPM sources.
scp -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net:/tmp/centreon-import.centos$CENTOS_VERSION.tar.gz" .
tar xzf centreon-import.centos$CENTOS_VERSION.tar.gz

# Prepare Dockerfile.
cd centreon-build/containers
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < ppm/ppm.Dockerfile.in > ppm/ppm.centos$CENTOS_VERSION.Dockerfile

# CentOS PPM image.
rm -rf centreon-import
cp -r ../../centreon-import/www/modules/centreon-import .
docker build --no-cache -t ci.int.centreon.com:5000/mon-ppm:centos$CENTOS_VERSION -f ppm/ppm.centos$CENTOS_VERSION.Dockerfile .
docker push ci.int.centreon.com:5000/mon-ppm:centos$CENTOS_VERSION
