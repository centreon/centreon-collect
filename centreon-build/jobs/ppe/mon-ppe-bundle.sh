#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull Centreon Web image.
docker pull ci.int.centreon.com:5000/mon-web:$DISTRIB

# Prepare Dockerfile.
cd centreon-build/containers
sed "s/@DISTRIB@/$DISTRIB/g" < ppe/ppe.Dockerfile.in > ppe/ppe.$DISTRIB.Dockerfile

# Build image.
docker build --no-cache -t ci.int.centreon.com:5000/mon-ppe:$DISTRIB -f ppe/ppe.$DISTRIB.Dockerfile .
docker push ci.int.centreon.com:5000/mon-ppe:$DISTRIB
