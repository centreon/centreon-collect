#!/bin/sh

sed s/@CENTOS_VERSION@/7/g < centreon-build/containers/dependencies.Dockerfile.in > centreon-build/containers/dependencies.centos7.Dockerfile
docker build -t ci.int.centreon.com:5000/mon-dependencies:centos7 -f centreon-build/containers/dependencies.centos7.Dockerfile centreon-build/containers
docker push ci.int.centreon.com:5000/mon-dependencies:centos7
