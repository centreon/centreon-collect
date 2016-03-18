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

# Fetch PPE sources.
scp -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net:/tmp/centreon-export.centos$CENTOS_VERSION.tar.gz" .
tar xzf centreon-export.centos$CENTOS_VERSION.tar.gz

# CentOS PPE image.
cd centreon-build/containers
rm -rf centreon-export
cp -r ../../centreon-export/www/modules/centreon-export .
docker build --no-cache -t ci.int.centreon.com:5000/mon-ppe:centos$CENTOS_VERSION -f ppe/ppe.centos$CENTOS_VERSION.Dockerfile .
docker push ci.int.centreon.com:5000/mon-ppe:centos$CENTOS_VERSION
