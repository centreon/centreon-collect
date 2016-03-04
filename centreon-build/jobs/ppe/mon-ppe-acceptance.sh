#!/bin/sh

set -e
set -x

# Pull mon-ppe image.
docker pull ci.int.centreon.com:5000/mon-ppe:centos6

# Start a container.
containerid=`docker run -d -t -p 80 ci.int.centreon.com:5000/mon-ppe:centos6`
port=`docker port "$containerid" 80 | cut -d : -f 2`

# Check that phantomjs is running.
nc -w 0 localhost 4444
if [ "$?" -ne 0 ] ; then
  screen -d -m phantomjs --webdriver=4444
fi

# Run acceptance tests.
export CENTREON_WEB_IMAGE=ci.int.centreon.com:5000/mon-ppe:centos6
rm -rf xunit-reports
cd centreon-export
composer install
composer update
/opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports"

# Stop container.
docker stop "$containerid"
