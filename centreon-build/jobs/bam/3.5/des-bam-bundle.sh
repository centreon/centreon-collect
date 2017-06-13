#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull base image.
WEB_IMAGE=ci.int.centreon.com:5000/mon-web:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
cd centreon-build/containers
sed "s/@DISTRIB@/$DISTRIB/g" < bam/Dockerfile.in > bam/$DISTRIB.Dockerfile

# Build image.
docker build --no-cache -t ci.int.centreon.com:5000/des-bam:$DISTRIB -f bam/$DISTRIB.Dockerfile .
docker push ci.int.centreon.com:5000/des-bam:$DISTRIB
