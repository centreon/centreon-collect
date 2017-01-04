#!/bin/sh

set -e
set -x

# Pull base image.
docker pull ubuntu:16.04

# Fetch middleware sources.
scp -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com:/srv/sources/centreon-imp-portal-api.tar.gz" .
tar xzf centreon-imp-portal-api.tar.gz

# CentOS middleware image.
cd centreon-build/containers
rm -rf centreon-imp-portal-api
cp -r ../../centreon-imp-portal-api .
docker build --no-cache -t ci.int.centreon.com:5000/mon-middleware:latest -f middleware/latest.Dockerfile .
docker push ci.int.centreon.com:5000/mon-middleware:latest
