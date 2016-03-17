#!/bin/sh

sed s/@CENTOS_VERSION@/6/g < centreon-build/containers/dependencies.Dockerfile.in > centreon-build/containers/dependencies.centos6.Dockerfile
docker build -t ci.int.centreon.com:5000/mon-dependencies:centos6 -f centreon-build/containers/dependencies.centos6.Dockerfile centreon-build/containers
docker push ci.int.centreon.com:5000/mon-dependencies:centos6
