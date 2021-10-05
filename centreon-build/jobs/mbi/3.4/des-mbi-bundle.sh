#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull base image.
DEP_IMAGE=registry.centreon.com/mon-dependencies-3.4:$DISTRIB
WEB_IMAGE=registry.centreon.com/mon-web-3.4:$DISTRIB
docker pull $DEP_IMAGE
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
cd centreon-build/containers
sed "s/@DISTRIB@/$DISTRIB/g" < mbi/server.Dockerfile.in > mbi/server.$DISTRIB.Dockerfile
sed "s/@DISTRIB@/$DISTRIB/g" < mbi/web.Dockerfile.in > mbi/web.$DISTRIB.Dockerfile

# Build image.
docker build --no-cache -t registry.centreon.com/des-mbi-server-3.4:$DISTRIB -f mbi/server.$DISTRIB.Dockerfile .
docker build --no-cache -t registry.centreon.com/des-mbi-web-3.4:$DISTRIB -f mbi/web.$DISTRIB.Dockerfile .
docker push registry.centreon.com/des-mbi-server-3.4:$DISTRIB
docker push registry.centreon.com/des-mbi-web-3.4:$DISTRIB
