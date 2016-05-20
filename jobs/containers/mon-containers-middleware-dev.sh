#!/bin/sh

set -e
set -x

# Check arguments.
CENTREON_BUILD=`dirname $0`/../..
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Prepare Dockerfile.
sed "s/@DISTRIB@/$DISTRIB/g" < "$CENTREON_BUILD/containers/middleware/middleware-dev.Dockerfile.in" > "$CENTREON_BUILD/containers/middleware/middleware.$DISTRIB.Dockerfile"

# CentOS middleware image.
rm -rf "$CENTREON_BUILD/containers/centreon-imp-portal-api"
cp -r . "$CENTREON_BUILD/containers/centreon-imp-portal-api"
docker build -t mon-middleware-dev:$DISTRIB -f "$CENTREON_BUILD/containers/middleware/middleware.$DISTRIB.Dockerfile" "$CENTREON_BUILD/containers"
