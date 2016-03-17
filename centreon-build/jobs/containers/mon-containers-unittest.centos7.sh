#!/bin/sh

sed s/@CENTOS_VERSION@/7/g < centreon-build/containers/unittest.Dockerfile.in > centreon-build/containers/unittest.centos7.Dockerfile
docker build -t ci.int.centreon.com:5000/mon-unittest:centos7 -f centreon-build/containers/unittest.centos7.Dockerfile centreon-build/containers
docker push ci.int.centreon.com:5000/mon-unittest:centos7
