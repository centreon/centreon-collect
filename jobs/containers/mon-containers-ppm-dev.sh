#!/bin/sh

set -e
set -x

# Check arguments.
CENTREON_BUILD=`dirname "$0"`/../..
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Prepare Dockerfile.
sed "s/@DISTRIB@/$DISTRIB/g" < "$CENTREON_BUILD/containers/ppm/ppm-dev.Dockerfile.in" > "$CENTREON_BUILD/containers/ppm/ppm-dev.$DISTRIB.Dockerfile"

# Build PPM image.
rm -rf "$CENTREON_BUILD/containers/centreon-pp-manager"
cp -r ./www/modules/centreon-pp-manager "$CENTREON_BUILD/containers/centreon-pp-manager"
docker build -t mon-ppm-dev:$DISTRIB -f "$CENTREON_BUILD/containers/ppm/ppm-dev.$DISTRIB.Dockerfile" "$CENTREON_BUILD/containers"
