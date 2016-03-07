#!/bin/sh

set -e
set -x

# Pull Centreon Web image.
docker pull ci.int.centreon.com:5000/mon-web:centos6

# Fetch PPE sources.
scp -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net:/tmp/centreon-export.tar.gz" .
tar xzf centreon-export.tar.gz

# CentOS 6 running image.
cd centreon-build/containers
rm -rf centreon-export
cp -r ../../centreon-export/www/modules/centreon-export .
docker build --no-cache -t ci.int.centreon.com:5000/mon-ppe:centos6 -f ppe/ppe.centos6.Dockerfile .
docker push ci.int.centreon.com:5000/mon-ppe:centos6
