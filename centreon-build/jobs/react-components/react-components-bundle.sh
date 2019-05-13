#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-react-components

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "react-components/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Target images.
REGISTRY="registry.centreon.com"
BASE_IMG="node:8-slim"
BUILT_IMG="$REGISTRY/react-components-$VERSION-$RELEASE:latest"

# Pull base image.
docker pull "$BASE_IMG"

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../containers centreon-build-containers
cd centreon-build-containers
sed "s#@BASE_IMAGE@#$BASE_IMG#g" < react-components/Dockerfile.in > react-components/Dockerfile
cp -R ../$PROJECT-$VERSION ./$PROJECT

# Build 'fresh' image.
docker build --no-cache -t "$BUILT_IMG" -f react-components/Dockerfile .
docker push "$BUILT_IMG"