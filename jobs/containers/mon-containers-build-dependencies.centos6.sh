#!/bin/sh

sed s/@CENTOS_VERSION@/6/g < centreon-build/containers/build-dependencies.Dockerfile.in > centreon-build/containers/build-dependencies.centos6.Dockerfile
docker build --no-cache -t ci.int.centreon.com:5000/mon-build-dependencies:centos6 -f centreon-build/containers/build-dependencies.centos6.Dockerfile centreon-build/containers
docker push ci.int.centreon.com:5000/mon-build-dependencies:centos6
