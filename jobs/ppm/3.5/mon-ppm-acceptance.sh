#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-pp-manager

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
WEBDRIVER_IMAGE=selenium/standalone-chrome:latest
PPM_IMAGE="ci.int.centreon.com:5000/mon-ppm-$VERSION-$RELEASE:$DISTRIB"
PPM1_IMAGE=ci.int.centreon.com:5000/mon-ppm1:$DISTRIB
SQUID_SIMPLE_IMAGE=ci.int.centreon.com:5000/mon-squid-simple:latest
SQUID_BASIC_AUTH_IMAGE=ci.int.centreon.com:5000/mon-squid-basic-auth:latest
docker pull $WEBDRIVER_IMAGE
docker pull $PPM_IMAGE
docker pull $PPM1_IMAGE
docker pull $SQUID_SIMPLE_IMAGE
docker pull $SQUID_BASIC_AUTH_IMAGE

# Get sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
wget "http://srvi-repo.int.centreon.com/sources/internal/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"
cd "$PROJECT-$VERSION"

# Prepare Docker compose file.
sed -e 's#@WEB_IMAGE@#'$PPM_IMAGE'#g' < `dirname $0`/../../containers/web/docker-compose.yml.in > docker-compose-ppm.yml
sed -e 's#@WEB_IMAGE@#'$PPM1_IMAGE'#g' < `dirname $0`/../../containers/web/docker-compose.yml.in > docker-compose-ppm1.yml
sed -e 's#@WEB_IMAGE@#'$PPM_IMAGE'#g' < `dirname $0`/../../containers/squid/simple/docker-compose.yml.in > docker-compose-ppm-squid-simple.yml
sed -e 's#@WEB_IMAGE@#'$PPM_IMAGE'#g' < `dirname $0`/../../containers/squid/basic-auth/docker-compose.yml.in > docker-compose-ppm-squid-basic-auth.yml

# Prepare behat.yml.
alreadyset=`grep docker-compose-ppm.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs\n      ppm: docker-compose-ppm.yml\n      ppm1: docker-compose-ppm1.yml\n      ppm_squid_simple: docker-compose-ppm-squid-simple.yml\n      ppm_squid_basic_auth: docker-compose-ppm-squid-basic-auth.yml#g' behat.yml
fi

# Filter tags
if [ "$DISTRIB" = "centos6" ] ; then
  EXCLUSION="NoneIsExcluded"
  TAGS='~@centos7only'
elif [ "$DISTRIB" = "centos7" ] ; then
  EXCLUSION="ModuleUpdate.feature"
  TAGS='~@centos6only'
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../acceptance-logs
mkdir ../acceptance-logs
composer install
composer update
ls features/*.feature | grep -v "$EXCLUSION" | parallel ./vendor/bin/behat --format=junit --tags "$TAGS" --out="../xunit-reports/{/.}" "{}" || true
