#!/bin/sh

# Pull main image.
docker pull ci.int.centreon.com:5000/mon-web:centos6

# Start a container.
containerid=`docker run -d -t -p 80 ci.int.centreon.com:5000/mon-web:centos6`
port=`docker port "$containerid" 80 | cut -d : -f 2`

# Prepare for acceptance tests run.
cd centreon-web

# Run acceptance tests.
export CENTREON_WEB_IMAGE=ci.int.centreon.com:5000/mon-web:centos6
/opt/behat/vendor/bin/behat --strict # --format junit --out ../mon-web-acceptance.xml

# Stop container.
docker stop "$containerid"
