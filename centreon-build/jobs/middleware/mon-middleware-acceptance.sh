#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-imp-portal-api

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Pull images.
REGISTRY="ci.int.centreon.com:5000"
WEBDRIVER_IMAGE=selenium/standalone-chrome:latest
MIDDLEWARE_IMAGE="$REGISTRY/mon-middleware-$VERSION-$RELEASE:latest"
REDIS_IMAGE=redis:latest
docker pull $WEBDRIVER_IMAGE
docker pull $MIDDLEWARE_IMAGE
docker pull $REDIS_IMAGE

# Get sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
get_internal_source "middleware/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"
cd "$PROJECT-$VERSION"

# Prepare Docker Compose file.
sed -e 's#@MIDDLEWARE_IMAGE@#'$MIDDLEWARE_IMAGE'#g' < `dirname $0`/../../containers/middleware/docker-compose-standalone.yml.in > docker-compose-middleware.yml

# Copy compose file of webdriver
cp `dirname $0`/../../../containers/webdrivers/docker-compose.yml.in docker-compose-webdriver.yml

# Prepare behat.yml.
alreadyset=`grep docker-compose-middleware.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs\n      middleware: docker-compose-middleware.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../acceptance-logs
mkdir ../acceptance-logs
composer install
docker-compose -f docker-compose-webdriver.yml -p webdriver up -d
ls features/*.feature | parallel ./vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}" || true
docker-compose -f docker-compose-webdriver.yml -p webdriver down -v
