#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
WEBDRIVER_IMAGE=ci.int.centreon.com:5000/mon-phantomjs:latest
PPM_IMAGE=ci.int.centreon.com:5000/mon-ppm:$DISTRIB
PPM1_IMAGE=ci.int.centreon.com:5000/mon-ppm1:$DISTRIB
SQUID_IMAGE=ci.int.centreon.com:5000/mon-squid:latest
docker pull $WEBDRIVER_IMAGE
docker pull $PPM_IMAGE
docker pull $PPM1_IMAGE
docker pull $SQUID_IMAGE

# Prepare Docker compose file.
cd centreon-pp-manager
sed -e 's#@WEB_IMAGE@#'$PPM_IMAGE'#g' < `dirname $0`/../../containers/web/docker-compose.yml.in > docker-compose-ppm.yml
sed -e 's#@WEB_IMAGE@#'$PPM1_IMAGE'#g' < `dirname $0`/../../containers/web/docker-compose.yml.in > docker-compose-ppm1.yml
sed -e 's#@WEB_IMAGE@#'$PPM_IMAGE'#g' < `dirname $0`/../../containers/squid/docker-compose.yml.in > docker-compose-ppm-squid.yml

# Prepare behat.yml.
alreadyset=`grep docker-compose-ppm.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs-wip\n      ppm: docker-compose-ppm.yml\n      ppm1: docker-compose-ppm1.yml\n      ppm_squid: docker-compose-ppm-squid.yml#g' behat.yml
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
rm -rf ../acceptance-logs-wip
mkdir ../acceptance-logs-wip
composer install
composer update
ls features/*.feature | grep -v "$EXCLUSION" | parallel /opt/behat/vendor/bin/behat --strict --format=junit --tags "$TAGS" --out="../xunit-reports/{/.}" "{}"
rm -rf ../acceptance-logs
mv ../acceptance-logs-wip ../acceptance-logs
