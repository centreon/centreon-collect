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
DEP_IMAGE=ci.int.centreon.com:5000/mon-dependencies:$DISTRIB
WEB_IMAGE=ci.int.centreon.com:5000/mon-web:$DISTRIB
docker pull $DEP_IMAGE
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
cd centreon-build/containers
sed "s/@DISTRIB@/$DISTRIB/g" < mbi/server.Dockerfile.in > mbi/server.$DISTRIB.Dockerfile
sed "s/@DISTRIB@/$DISTRIB/g" < mbi/web.Dockerfile.in > mbi/web.$DISTRIB.Dockerfile

# Build image.
docker build --no-cache -t ci.int.centreon.com:5000/des-mbi-server:$DISTRIB -f mbi/server.$DISTRIB.Dockerfile .
docker build --no-cache -t ci.int.centreon.com:5000/des-mbi-web:$DISTRIB -f mbi/web.$DISTRIB.Dockerfile .
docker push ci.int.centreon.com:5000/des-mbi-server:$DISTRIB
docker push ci.int.centreon.com:5000/des-mbi-web:$DISTRIB
