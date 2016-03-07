#!/bin/sh

set -e
set -x

# Pull monitoring-running image.
docker pull ci.int.centreon.com:5000/mon-web:centos6

# Fetch LM sources.
scp -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net:/tmp/centreon-license-manager.tar.gz" .
tar xzf centreon-license-manager.tar.gz

# CentOS 6 running image.
cd centreon-build/containers/
rm -rf centreon-license-manager
cp -R ../../centreon-license-manager/www/modules/centreon-license-manager .
docker build --no-cache -t ci.int.centreon.com:5000/mon-lm:centos6 -f lm/lm.centos6.Dockerfile .
docker push ci.int.centreon.com:5000/mon-lm:centos6
