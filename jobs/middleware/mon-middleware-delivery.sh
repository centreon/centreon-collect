#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Set Docker images as latest.
REGISTRY='ci.int.centreon.com:5000'
docker pull "$REGISTRY/mon-middleware-$VERSION-$RELEASE:latest"
docker tag "$REGISTRY/mon-middleware-$VERSION-$RELEASE:latest" "$REGISTRY/mon-middleware:latest"
docker push "$REGISTRY/mon-middleware:latest"

# Generate Docker Compose file.
sed -e "s#@MIDDLEWARE_IMAGE@#$MIDDLEWARE_IMAGE#g" -e 's/3000/3000:3000/g' -e 's/3306/33060:3306/g' < `dirname $0`/../../containers/middleware/docker-compose-standalone.yml.in > /opt/middleware/docker-compose.yml

# Update container.
cd /opt/middleware
docker-compose up -d
