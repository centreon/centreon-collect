#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull image.
MIDDLEWARE_IMAGE=ci.int.centreon.com:5000/mon-middleware:$DISTRIB
docker pull $MIDDLEWARE_IMAGE

# Check that phantomjs is running.
export PHANTOMJS_RUNNING=1
nc -w 0 localhost 4444 || export PHANTOMJS_RUNNING=0 || true
if [ "$PHANTOMJS_RUNNING" -ne 1 ] ; then
  screen -d -m phantomjs --webdriver=4444
fi

# Checkout centreon-build
if [ \! -d centreon-build ] ; then
  git clone ssh://github.com/centreon/centreon-build
fi
cp centreon-build/jobs/middleware/public.asc centreon-imp-portal-api

# Prepare Docker compose file.
cd centreon-imp-portal-api
sed 's#@MIDDLEWARE_IMAGE@#'$MIDDLEWARE_IMAGE'#g' < `dirname $0`/../../containers/middleware/docker-compose-standalone.yml.in > docker-compose-middleware.yml

# Prepare behat.yml.
alreadyset=`grep docker-compose-middleware.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      middleware: docker-compose-middleware.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
composer install
composer update
ls features/*.feature | parallel /opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}"
