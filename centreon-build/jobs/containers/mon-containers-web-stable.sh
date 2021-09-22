#!/bin/sh

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Prepare Dockerfiles.
cd centreon-build/containers
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < web/stable.Dockerfile.in > web/stable.centos$CENTOS_VERSION.Dockerfile

# Build 'stable' image.
docker build --no-cache --ulimit 'nofile=40000' -t registry.centreon.com/mon-web-stable:centos$CENTOS_VERSION -f web/stable.centos$CENTOS_VERSION.Dockerfile .
docker push registry.centreon.com/mon-web-stable:centos$CENTOS_VERSION
