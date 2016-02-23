#!/bin/sh

# Launch monitoring-unitttest container.
docker pull 10.24.11.199:5000/monitoring-unittest:centos6
containerid=`docker create 10.24.11.199:5000/monitoring-unittest:centos6 /usr/local/bin/unittest-ppe`

# Copy sources to container.
if [ \! -d centreon-web ] ; then
  git clone https://github.com/centreon/centreon centreon-web
fi
docker cp centreon-web "$containerid:/usr/local/src/centreon-web"
docker cp centreon-export/www/modules/centreon-export "$containerid:/usr/local/src/centreon-web/www/modules/centreon-export"

# Run unit tests.
docker start -a "$containerid"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
