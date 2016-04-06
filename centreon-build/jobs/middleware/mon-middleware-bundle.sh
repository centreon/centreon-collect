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

# Fetch middleware sources.
scp -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net:/tmp/centreon-imp-portal-api.centos$CENTOS_VERSION.tar.gz" .
tar xzf centreon-imp-portal-api.centos$CENTOS_VERSION.tar.gz

# Prepare Dockerfile.
cd centreon-build/containers
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < middleware/middleware.Dockerfile.in > middleware/middleware.centos$CENTOS_VERSION.Dockerfile

# CentOS middleware image.
rm -rf centreon-imp-portal-api
cp -r ../../centreon-imp-portal-api .
docker build --no-cache -t ci.int.centreon.com:5000/mon-middleware:centos$CENTOS_VERSION -f middleware/middleware.centos$CENTOS_VERSION.Dockerfile .
docker push ci.int.centreon.com:5000/mon-middleware:centos$CENTOS_VERSION
