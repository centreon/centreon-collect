#!/bin/sh

# Pull monitoring-running image.
docker pull ci.int.centreon.com:5000/mon-web:centos6
docker tag -f ci.int.centreon.com:5000/mon-web:centos6 mon-web:centos6

# Fetch LM sources.
scp -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net:/tmp/centreon-license-manager.tar.gz" .
tar xzf centreon-license-manager.tar.gz

# CentOS 6 running image.
cd centreon-build/containers/centos6/lm/
rm -rf centreon-license-manager
cp ../../../../centreon-license-manager/www/modules/centreon-license-manager .
docker build -t mon-lm-running:centos6 -f lm-running.Dockerfile
docker tag -f mon-lm-running:centos6 10.24.11.199:5000/mon-lm-running:centos6
docker push 10.24.11.199:5000/mon-lm-running:centos6
