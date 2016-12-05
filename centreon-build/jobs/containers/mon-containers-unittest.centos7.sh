#!/bin/sh

sed s/@CENTOS_VERSION@/7/g < centreon-build/containers/unittest/unittest.Dockerfile.in > centreon-build/containers/unittest/unittest.centos7.Dockerfile
docker pull ci.int.centreon.com:5000/mon-dependencies:centos7
docker build --no-cache -t ci.int.centreon.com:5000/mon-unittest:centos7 -f centreon-build/containers/unittest/unittest.centos7.Dockerfile centreon-build/containers
docker push ci.int.centreon.com:5000/mon-unittest:centos7
