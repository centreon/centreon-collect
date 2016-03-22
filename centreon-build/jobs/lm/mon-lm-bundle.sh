#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Pull monitoring-running image.
docker pull ci.int.centreon.com:5000/mon-web:centos$CENTOS_VERSION

# Fetch LM sources.
scp -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net:/tmp/centreon-license-manager.centos$CENTOS_VERSION.tar.gz" .
tar xzf centreon-license-manager.centos$CENTOS_VERSION.tar.gz

# CentOS 6 running image.
cd centreon-build/containers/
rm -rf centreon-license-manager
cp -R ../../centreon-license-manager/www/modules/centreon-license-manager .
docker build --no-cache -t ci.int.centreon.com:5000/mon-lm:centos$CENTOS_VERSION -f lm/lm.centos$CENTOS_VERSION.Dockerfile .
docker push ci.int.centreon.com:5000/mon-lm:centos$CENTOS_VERSION
