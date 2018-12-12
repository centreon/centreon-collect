#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-hub-ui

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Pull images.
REGISTRY="registry.centreon.com"
HUB_IMAGE="$REGISTRY/hub-$VERSION-$RELEASE:latest"
MIDDLEWARE_IMAGE="$REGISTRY/mon-middleware-dataset:latest"
REDIS_IMAGE=redis:latest
OPENLDAP_IMAGE="$REGISTRY/mon-openldap:latest"
docker pull $HUB_IMAGE
docker pull $MIDDLEWARE_IMAGE
docker pull $REDIS_IMAGE
docker pull $OPENLDAP_IMAGE

# Get sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
get_internal_source "hub/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Prepare Docker Compose file.
sed -e 's#@WEB_IMAGE@#'$HUB_IMAGE'#g' -e 's#@MIDDLEWARE_IMAGE@#'$MIDDLEWARE_IMAGE'#g' < `dirname $0`/../../containers/middleware/docker-compose-web.yml.in > docker-compose-hub.yml

# Copy compose file of webdriver
cp `dirname $0`/../../containers/webdrivers/docker-compose.yml.in "$PROJECT-$VERSION/docker-compose-webdriver.yml"

# Prepare behat.yml.
cd "$PROJECT-$VERSION"
alreadyset=`grep docker-compose-middleware.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs\n      hub: docker-compose-hub.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../acceptance-logs
mkdir ../acceptance-logs
composer install
launch_webdriver docker-compose-webdriver.yml
ls features/*.feature | parallel ./vendor/bin/behat --format=pretty --out=std --format=junit --out="../xunit-reports/{/.}" "{}" || true
stop_webdriver docker-compose-webdriver.yml
