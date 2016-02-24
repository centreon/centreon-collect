#!/bin/sh

# Pull monitoring-running image.
docker pull 10.24.11.199:5000/monitoring-running:centos6
docker tag -f 10.24.11.199:5000/monitoring-running:centos6 monitoring-running:centos6

# Fetch LM sources.
scp -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net:/tmp/centreon-license-manager.tar.gz" .
tar xzf centreon-license-manager.tar.gz

# CentOS 6 running image.
cd centreon-build/containers/centos6
rm -rf centreon-license-manager
cp ../../../centreon-license-manager/www/modules/centreon-license-manager .
docker build -t monitoring-lm-running:centos6 -f lm-running.Dockerfile
docker tag -f monitoring-lm-running:centos6 10.24.11.199:5000/monitoring-lm-running:centos6
docker push 10.24.11.199:5000/monitoring-lm-running:centos6
