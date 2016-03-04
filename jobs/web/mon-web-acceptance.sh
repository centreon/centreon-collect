#!/bin/sh

set -e
set -x

# Pull main image.
docker pull ci.int.centreon.com:5000/mon-web:centos6

# Start a container.
containerid=`docker run -d -t -p 80 ci.int.centreon.com:5000/mon-web:centos6`
port=`docker port "$containerid" 80 | cut -d : -f 2`

# Check that phantomjs is running.
export PHANTOMJS_RUNNING=1
nc -w 0 localhost 4444 || export PHANTOMJS_RUNNING=0 || true
if [ "$PHANTOMJS_RUNNING" -ne 1 ] ; then
  screen -d -m phantomjs --webdriver=4444
fi

# Run acceptance tests.
export CENTREON_WEB_IMAGE=ci.int.centreon.com:5000/mon-web:centos6
rm -rf xunit-reports
cd centreon-web
/opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports"

# Stop container.
docker stop "$containerid"
