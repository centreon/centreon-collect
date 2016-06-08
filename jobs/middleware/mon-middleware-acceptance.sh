#!/bin/sh

set -e
set -x

# Pull image.
WEBDRIVER_IMAGE=ci.int.centreon.com:5000/mon-phantomjs:latest
MIDDLEWARE_IMAGE=ci.int.centreon.com:5000/mon-middleware:latest
docker pull $WEBDRIVER_IMAGE
docker pull $MIDDLEWARE_IMAGE

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
ls features/*.feature | parallel -j 1 /opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}"
