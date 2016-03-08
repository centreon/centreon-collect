#!/bin/sh

set -e
set -x

# Launch mon-unitttest container.
docker pull ci.int.centreon.com:5000/mon-unittest:centos6
containerid=`docker create ci.int.centreon.com:5000/mon-unittest:centos6 /usr/local/bin/unittest-ppe`

# Copy sources to container.
if [ \! -d centreon-web ] ; then
  git clone https://github.com/centreon/centreon centreon-web
fi
cd centreon-web
git checkout origin/2.8.x
cd ..
docker cp centreon-web "$containerid:/usr/local/src/centreon-web"
docker cp centreon-export/www/modules/centreon-export "$containerid:/usr/local/src/centreon-web/www/modules/centreon-export"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/centreon-export.xml" centreon-export.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
