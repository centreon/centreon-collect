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

# Build middleware dataset image
MIDDLEWARE_DATASET_IMAGE="$REGISTRY/mon-middleware-dataset:latest"
docker build --no-cache -t "$MIDDLEWARE_DATASET_IMAGE" -f middleware/middleware-dataset.Dockerfile .
docker push "$MIDDLEWARE_DATASET_IMAGE"

# Generate Docker Compose file.
sed -e "s#@MIDDLEWARE_IMAGE@#$REGISTRY/mon-middleware:latest#g" -e 's/3000/3000:3000/g' -e 's/3306/3306:3306/g' < `dirname $0`/../../containers/middleware/docker-compose-standalone.yml.in > docker-compose.yml

# Update container.
docker-compose up -d
