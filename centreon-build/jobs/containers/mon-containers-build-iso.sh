#!/bin/sh

set -e
set -x

# CentOS 6.
if [ "$1" = "centos6" ] ; then
  # Pull base image.
  docker pull ubuntu:latest

  # Build image.
  cd `dirname $0`/../../containers
  docker build -t ci.int.centreon.com:5000/mon-build-iso:centos6 -f iso/centos7/build-iso.Dockerfile .

  # Push image.
  docker push ci.int.centreon.com:5000/mon-build-iso:centos6

# CentOS 7.
elif [ "$1" = "centos7" ] ; then
  # Pull base image.
  docker pull centos:7.4.1708

  # Build image.
  cd `dirname $0`/../../containers
  docker build -t ci.int.centreon.com:5000/mon-build-iso:centos7 -f iso/centos7/build-iso.Dockerfile .

  # Push image.
  docker push ci.int.centreon.com:5000/mon-build-iso:centos7

# Unknown distrib.
else
  echo "Unsupported distribution $1"
  exit 1
fi
