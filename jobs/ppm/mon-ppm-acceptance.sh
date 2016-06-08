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
MIDDLEWARE_IMAGE=ci.int.centreon.com:5000/mon-middleware:latest
docker pull $WEBDRIVER_IMAGE
docker pull $PPM_IMAGE
docker pull $MIDDLEWARE_IMAGE

# Prepare Docker compose file.
cd centreon-import
sed -e 's#@WEB_IMAGE@#'$PPM_IMAGE'#g' -e 's#@MIDDLEWARE_IMAGE@#'$MIDDLEWARE_IMAGE'#g' < `dirname $0`/../../containers/middleware/docker-compose-web.yml.in > docker-compose-ppm.yml

# Prepare behat.yml.
alreadyset=`grep docker-compose-ppm.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      ppm: docker-compose-ppm.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
composer install
composer update
ls features/*.feature | parallel /opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}"
