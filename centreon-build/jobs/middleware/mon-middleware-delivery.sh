#!/bin/sh

set -e
set -x

# Pull new image.
MIDDLEWARE_IMAGE=ci.int.centreon.com:5000/mon-middleware:latest
docker pull $MIDDLEWARE_IMAGE

# Generate Docker Compose file.
sed "s#@MIDDLEWARE_IMAGE@#$MIDDLEWARE_IMAGE#g" < `dirname $0`/../../containers/middleware/docker-compose-standalone.yml.in > /opt/middleware/docker-compose.yml

# Update container.
cd /opt/middleware
docker-compose up -d
