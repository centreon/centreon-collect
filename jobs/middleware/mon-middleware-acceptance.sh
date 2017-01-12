#!/bin/sh

set -e
set -x

# Pull image.
WEBDRIVER_IMAGE=ci.int.centreon.com:5000/mon-phantomjs:latest
MIDDLEWARE_IMAGE=ci.int.centreon.com:5000/mon-middleware:latest
REDIS_IMAGE=redis:latest
docker pull $WEBDRIVER_IMAGE
docker pull $MIDDLEWARE_IMAGE
docker pull $REDIS_IMAGE

# Copy test public key.
cp `dirname $0`/public.asc centreon-imp-portal-api

# Prepare Docker compose file.
cd centreon-imp-portal-api
sed 's#@MIDDLEWARE_IMAGE@#'$MIDDLEWARE_IMAGE'#g' < `dirname $0`/../../containers/middleware/docker-compose-standalone.yml.in > docker-compose-middleware.yml

# Prepare behat.yml.
alreadyset=`grep docker-compose-middleware.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs-wip\n      middleware: docker-compose-middleware.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../acceptance-logs-wip
mkdir ../acceptance-logs-wip
composer self-update
composer install
composer update
ls features/*.feature | parallel -j 1 /opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}"
rm -rf ../acceptance-logs
mv ../acceptance-logs-wip ../acceptance-logs
