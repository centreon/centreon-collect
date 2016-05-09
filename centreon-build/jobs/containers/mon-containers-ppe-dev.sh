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

# Prepare Dockerfile.
cd centreon-build/containers
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < ppe/ppe.Dockerfile.in > ppe/ppe.centos$CENTOS_VERSION.Dockerfile

# CentOS PPE image.
rm -rf centreon-export
cp -r ../../centreon-export/www/modules/centreon-export .
docker build --no-cache -t mon-ppe-dev:centos$CENTOS_VERSION -f ppe/ppe.centos$CENTOS_VERSION.Dockerfile .
