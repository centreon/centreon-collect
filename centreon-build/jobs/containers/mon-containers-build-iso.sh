#!/bin/sh

set -e
set -x

# CentOS 6.
if [ "$1" = "centos6" ] ; then
  # Pull base image.
  docker pull ubuntu:latest

  # Build image.
  cd `dirname $0`/../../containers
  docker build -t registry.centreon.com/mon-build-iso:centos6 -f iso/centos6/build-iso.Dockerfile .

  # Push image.
  docker push registry.centreon.com/mon-build-iso:centos6

# CentOS 7.
elif [ "$1" = "centos7" ] ; then
  # Pull base image.
  docker pull registry.centreon.com/centos:7.5.1804

  # Build image.
  cd `dirname $0`/../../containers
  docker build -t registry.centreon.com/mon-build-iso:centos7 -f iso/centos7/build-iso.Dockerfile .

  # Push image.
  docker push registry.centreon.com/mon-build-iso:centos7

# Unknown distrib.
else
  echo "Unsupported distribution $1"
  exit 1
fi
