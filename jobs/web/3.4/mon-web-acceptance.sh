#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7> [tags]"
  exit 1
fi
DISTRIB="$1"
TESTTAGS="$2"
if [ -z "$TESTTAGS" ] ; then
  TESTTAGS='@tag,~@tag'
fi

# Pull images.
WEB_IMAGE="$REGISTRY/mon-web-$VERSION-$RELEASE:$DISTRIB"
WEB_FRESH_IMAGE="$REGISTRY/mon-web-fresh-$VERSION-$RELEASE:$DISTRIB"
WEB_WIDGETS_IMAGE="$REGISTRY/mon-web-widgets-$VERSION-$RELEASE:$DISTRIB"
MEDIAWIKI_IMAGE="$REGISTRY/mon-mediawiki-3.4:latest"
OPENLDAP_IMAGE="$REGISTRY/mon-openldap:latest"
INFLUXDB_IMAGE="$REGISTRY/influxdb:latest"
NEWMAN_IMAGE="$REGISTRY/postman/newman_alpine33:latest"
docker pull $WEB_IMAGE
docker pull $WEB_FRESH_IMAGE
docker pull $WEB_WIDGETS_IMAGE
docker pull $MEDIAWIKI_IMAGE
docker pull $OPENLDAP_IMAGE
docker pull $INFLUXDB_IMAGE
docker pull $NEWMAN_IMAGE

# Fetch sources.
rm -rf "centreon-$VERSION" "centreon-$VERSION.tar.gz"
get_internal_source "web/centreon-web-$VERSION-$RELEASE/centreon-$VERSION.tar.gz"
tar xzf "centreon-$VERSION.tar.gz"

# Prepare Docker Compose file.
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/web/3.4/docker-compose.yml.in > "centreon-$VERSION/docker-compose-web.yml"
sed 's#@WEB_IMAGE@#'$WEB_FRESH_IMAGE'#g' < `dirname $0`/../../../containers/web/3.4/docker-compose.yml.in > "centreon-$VERSION/docker-compose-web-fresh.yml"
sed 's#@WEB_IMAGE@#'$WEB_WIDGETS_IMAGE'#g' < `dirname $0`/../../../containers/web/3.4/docker-compose.yml.in > "centreon-$VERSION/docker-compose-web-widgets.yml"
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/squid/simple/docker-compose.yml.in > "centreon-$VERSION/docker-compose-web-squid-simple.yml"
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/squid/basic-auth/docker-compose.yml.in > "centreon-$VERSION/docker-compose-web-squid-basic-auth.yml"
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/mediawiki/3.4/docker-compose.yml.in > "centreon-$VERSION/docker-compose-web-kb.yml"
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/openldap/docker-compose.yml.in > "centreon-$VERSION/docker-compose-web-openldap.yml"
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/web/3.4/docker-compose-influxdb.yml.in > "centreon-$VERSION/docker-compose-web-influxdb.yml"

# Prepare Behat.yml
cd "centreon-$VERSION"
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
ls features/*.feature | parallel ./vendor/bin/behat --tags "$TESTTAGS" --format=pretty --out=std --format=junit --out="../xunit-reports/{/.}" "{}" || true
