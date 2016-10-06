#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
WEB_IMAGE=ci.int.centreon.com:5000/mon-web:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfile.
cd centreon-build/containers
sed "s/@DISTRIB@/$DISTRIB/g" < automation/automation.Dockerfile.in > automation/automation.$DISTRIB.Dockerfile

# Build image.
docker build --no-cache -t ci.int.centreon.com:5000/mon-automation:$DISTRIB -f automation/automation.$DISTRIB.Dockerfile .
docker push ci.int.centreon.com:5000/mon-automation:$DISTRIB
