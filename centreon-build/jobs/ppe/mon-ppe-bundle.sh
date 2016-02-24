#!/bin/sh

# Pull monitoring-running image.
docker pull 10.24.11.199:5000/monitoring-running:centos6
docker tag -f 10.24.11.199:5000/monitoring-running:centos6 monitoring-running:centos6

# Fetch PPE sources.
scp -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net:/tmp/centreon-export.tar.gz" .
tar xzf centreon-export.tar.gz

# CentOS 6 running image.
cd centreon-build/containers/centos6/ppe/
rm -rf centreon-export
cp -r ../../../../centreon-export/www/modules/centreon-export .
docker build -t monitoring-ppe-running:centos6 -f ppe-running.Dockerfile .
docker tag -f monitoring-ppe-running:centos6 10.24.11.199:5000/monitoring-ppe-running:centos6
docker push 10.24.11.199:5000/monitoring-ppe-running:centos6
