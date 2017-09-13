#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-web

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
WEB_IMAGE="ci.int.centreon.com:5000/mon-web-$VERSION-$RELEASE:$DISTRIB"
WEB_FRESH_IMAGE="ci.int.centreon.com:5000/mon-web-fresh-$VERSION-$RELEASE:$DISTRIB"
WEB_WIDGETS_IMAGE="ci.int.centreon.com:5000/mon-web-widgets-$VERSION-$RELEASE:$DISTRIB"
MEDIAWIKI_IMAGE=ci.int.centreon.com:5000/mon-mediawiki:latest
OPENLDAP_IMAGE=ci.int.centreon.com:5000/mon-openldap:latest
INFLUXDB_IMAGE=influxdb:latest
docker pull $WEB_IMAGE
docker pull $WEB_FRESH_IMAGE
docker pull $WEB_WIDGETS_IMAGE
docker pull $MEDIAWIKI_IMAGE
docker pull $OPENLDAP_IMAGE
docker pull $INFLUXDB_IMAGE

# Fetch sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
get_internal_source "web/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"
cd "$PROJECT-$VERSION"

# Prepare Docker Compose file.
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/web/3.5/docker-compose.yml.in > docker-compose-web.yml
sed 's#@WEB_IMAGE@#'$WEB_FRESH_IMAGE'#g' < `dirname $0`/../../../containers/web/3.5/docker-compose.yml.in > docker-compose-web-fresh.yml
sed 's#@WEB_IMAGE@#'$WEB_WIDGETS_IMAGE'#g' < `dirname $0`/../../../containers/web/3.5/docker-compose.yml.in > docker-compose-web-widgets.yml
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/squid/simple/docker-compose.yml.in > docker-compose-web-squid-simple.yml
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/squid/basic-auth/docker-compose.yml.in > docker-compose-web-squid-basic-auth.yml
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/mediawiki/docker-compose.yml.in > docker-compose-web-kb.yml
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/openldap/docker-compose.yml.in > docker-compose-web-openldap.yml
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/web/3.5/docker-compose-influxdb.yml.in > docker-compose-web-influxdb.yml

# Copy compose file of webdriver
cp `dirname $0`/../../../containers/webdrivers/docker-compose.yml.in docker-compose-webdriver.yml

# Prepare Behat.yml
alreadyset=`grep docker-compose-web.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs\n      web: docker-compose-web.yml\n      web_fresh: docker-compose-web-fresh.yml\n      web_widgets: docker-compose-web-widgets.yml\n      web_squid_simple: docker-compose-web-squid-simple.yml\n      web_squid_basic_auth: docker-compose-web-squid-basic-auth.yml\n      web_kb: docker-compose-web-kb.yml\n      web_openldap: docker-compose-web-openldap.yml\n      web_influxdb: docker-compose-web-influxdb.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../acceptance-logs
mkdir ../acceptance-logs
composer install
export COMPOSE_HTTP_TIMEOUT=120
docker-compose -f docker-compose-webdriver.yml -p webdriver up -d
docker-compose -f docker-compose-webdriver.yml -p webdriver scale 'chrome=4'
ls features/*.feature | parallel ./vendor/bin/behat --format=pretty --out=std --format=junit --out="../xunit-reports/{/.}" "{}" || true
docker-compose -f docker-compose-webdriver.yml -p webdriver down -v
