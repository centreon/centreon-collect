#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-pp-manager

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
PPM_IMAGE="ci.int.centreon.com:5000/mon-ppm-$VERSION-$RELEASE:$DISTRIB"
PPM_AUTODISCO_IMAGE=ci.int.centreon.com:5000/mon-ppm-autodisco-$VERSION-$RELEASE:$DISTRIB
SQUID_SIMPLE_IMAGE=ci.int.centreon.com:5000/mon-squid-simple:latest
SQUID_BASIC_AUTH_IMAGE=ci.int.centreon.com:5000/mon-squid-basic-auth:latest
docker pull $PPM_IMAGE
docker pull $PPM_AUTODISCO_IMAGE
docker pull $SQUID_SIMPLE_IMAGE
docker pull $SQUID_BASIC_AUTH_IMAGE

# Get sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
get_internal_source "ppm/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Prepare Docker compose file.
sed -e 's#@WEB_IMAGE@#'$PPM_IMAGE'#g' < `dirname $0`/../../../containers/web/19.4/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-ppm.yml"
sed -e 's#@WEB_IMAGE@#'$PPM_AUTODISCO_IMAGE'#g' < `dirname $0`/../../../containers/web/19.4/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-ppm-autodisco.yml"
sed -e 's#@WEB_IMAGE@#'$PPM_IMAGE'#g' < `dirname $0`/../../../containers/squid/simple/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-ppm-squid-simple.yml"
sed -e 's#@WEB_IMAGE@#'$PPM_IMAGE'#g' < `dirname $0`/../../../containers/squid/basic-auth/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-ppm-squid-basic-auth.yml"

# Copy compose file of webdriver
cp `dirname $0`/../../../containers/webdrivers/docker-compose.yml.in "$PROJECT-$VERSION/docker-compose-webdriver.yml"

# Prepare behat.yml.
cd "$PROJECT-$VERSION"
alreadyset=`grep docker-compose-ppm.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs\n      ppm: docker-compose-ppm.yml\n      ppm_autodisco: docker-compose-ppm-autodisco.yml\n      ppm_squid_simple: docker-compose-ppm-squid-simple.yml\n      ppm_squid_basic_auth: docker-compose-ppm-squid-basic-auth.yml#g' behat.yml
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
