#!/bin/sh

set -e
set -x

# Launch mon-unitttest container.
docker pull ci.int.centreon.com:5000/mon-unittest:centos6
containerid=`docker create ci.int.centreon.com:5000/mon-unittest:centos6 /usr/local/bin/unittest-ppe`

# Install project dependencies.
cd centreon-export
composer install
composer update
cd ..

# Copy sources to container.
docker cp centreon-export "$containerid:/usr/local/src/centreon-export"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/centreon-export.xml" centreon-export.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
