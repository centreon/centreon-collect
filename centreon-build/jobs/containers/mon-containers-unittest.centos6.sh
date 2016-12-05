#!/bin/sh

sed s/@CENTOS_VERSION@/6/g < centreon-build/containers/unittest/unittest.Dockerfile.in > centreon-build/containers/unittest/unittest.centos6.Dockerfile
docker pull ci.int.centreon.com:5000/mon-dependencies:centos6
docker build --no-cache -t ci.int.centreon.com:5000/mon-unittest:centos6 -f centreon-build/containers/unittest/unittest.centos6.Dockerfile centreon-build/containers
docker push ci.int.centreon.com:5000/mon-unittest:centos6
