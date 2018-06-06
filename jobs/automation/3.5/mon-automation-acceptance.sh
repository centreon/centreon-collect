#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-automation

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|...>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
REGISTRY="ci.int.centreon.com:5000"
AUTOMATION_IMAGE="$REGISTRY/mon-automation-$VERSION-$RELEASE:$DISTRIB"
docker pull $AUTOMATION_IMAGE

# Get sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
get_internal_source "automation-web/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Prepare Docker compose file.
sed -e 's#@WEB_IMAGE@#'$AUTOMATION_IMAGE'#g' < `dirname $0`/../../../containers/web/3.5/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-automation.yml"

# Copy compose file of webdriver
cp `dirname $0`/../../../containers/webdrivers/docker-compose.yml.in "$PROJECT-$VERSION/docker-compose-webdriver.yml"

# Prepare behat.yml.
cd "$PROJECT-$VERSION"
alreadyset=`grep docker-compose-automation.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs\n      automation: docker-compose-automation.yml#g' behat.yml
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
