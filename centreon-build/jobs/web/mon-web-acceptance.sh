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
WEB_IMAGE=ci.int.centreon.com:5000/mon-web:$DISTRIB
WEB_FRESH_IMAGE=ci.int.centreon.com:5000/mon-web-fresh:$DISTRIB
docker pull $WEBDRIVER_IMAGE
docker pull $WEB_IMAGE
docker pull $WEB_FRESH_IMAGE

# Prepare Docker Compose file.
cd centreon-web
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../containers/web/docker-compose.yml.in > docker-compose-web.yml
sed 's#@WEB_IMAGE@#'$WEB_FRESH_IMAGE'#g' < `dirname $0`/../../containers/web/docker-compose.yml.in > docker-compose-web-fresh.yml

# Prepare Behat.yml
alreadyset=`grep docker-compose-web.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      web: docker-compose-web.yml\n      web_fresh: docker-compose-web-fresh.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
composer install
composer update
ls features/*.feature | parallel /opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}"
