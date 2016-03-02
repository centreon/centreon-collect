#!/bin/sh

# Pull mon-ppe image.
docker pull ci.int.centreon.com:5000/mon-ppe:centos6

# Start a container.
containerid=`docker run -d -t -p 80 ci.int.centreon.com:5000/mon-ppe:centos6`
port=`docker port "$containerid" 80 | cut -d : -f 2`

# Run acceptance tests.
export CENTREON_WEB_IMAGE=ci.int.centreon.com:5000/mon-ppe:centos6
rm -rf xunit-reports
cd centreon-export/www/modules/centreon-export
/opt/behat/vendor/bin/behat --strict --format=junit --out="../../../../xunit-reports"

# Stop container.
docker stop "$containerid"
