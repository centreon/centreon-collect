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
if [ "$#" -lt 2 ] ; then
  echo "USAGE: $0 <centos7|...> [feature]"
  exit 1
fi
DISTRIB="$1"

# Pull images.
WEB_IMAGE="registry.centreon.com/mon-web-$VERSION-$RELEASE:$DISTRIB"
WEB_FRESH_IMAGE="registry.centreon.com/mon-web-fresh-$VERSION-$RELEASE:$DISTRIB"
WEB_WIDGETS_IMAGE="registry.centreon.com/mon-web-widgets-$VERSION-$RELEASE:$DISTRIB"
MEDIAWIKI_IMAGE=registry.centreon.com/mon-mediawiki-20.04:latest
OPENLDAP_IMAGE=registry.centreon.com/mon-openldap:latest
PROXY_IMAGE=registry.centreon.com/mon-squid-simple:latest
INFLUXDB_IMAGE=influxdb:latest
NEWMAN_IMAGE=postman/newman_alpine33:latest
docker pull $WEB_IMAGE
docker pull $WEB_FRESH_IMAGE
docker pull $WEB_WIDGETS_IMAGE
docker pull $MEDIAWIKI_IMAGE
docker pull $OPENLDAP_IMAGE
docker pull $PROXY_IMAGE
docker pull $INFLUXDB_IMAGE
docker pull $NEWMAN_IMAGE

# Fetch sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz" "vendor.tar.gz"
get_internal_source "web/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
get_internal_source "web/$PROJECT-$VERSION-$RELEASE/vendor.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"
cd "$PROJECT-$VERSION"
rm -rf vendor
tar xzf "../vendor.tar.gz"
cd ..

# Prepare Docker Compose file.
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/web/20.04/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-web.yml"
sed 's#@WEB_IMAGE@#'$WEB_FRESH_IMAGE'#g' < `dirname $0`/../../../containers/web/20.04/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-web-fresh.yml"
sed 's#@WEB_IMAGE@#'$WEB_WIDGETS_IMAGE'#g' < `dirname $0`/../../../containers/web/20.04/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-web-widgets.yml"
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/squid/simple/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-web-squid-simple.yml"
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/squid/basic-auth/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-web-squid-basic-auth.yml"
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/mediawiki/20.04/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-web-kb.yml"
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/openldap/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-web-openldap.yml"
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/web/20.04/docker-compose-influxdb.yml.in > "$PROJECT-$VERSION/docker-compose-web-influxdb.yml"

# Prepare Behat.yml
cd "$PROJECT-$VERSION"
alreadyset=`grep docker-compose-web.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs\n      web: docker-compose-web.yml\n      web_fresh: docker-compose-web-fresh.yml\n      web_widgets: docker-compose-web-widgets.yml\n      web_squid_simple: docker-compose-web-squid-simple.yml\n      web_squid_basic_auth: docker-compose-web-squid-basic-auth.yml\n      web_kb: docker-compose-web-kb.yml\n      web_openldap: docker-compose-web-openldap.yml\n      web_influxdb: docker-compose-web-influxdb.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../acceptance-logs
mkdir ../acceptance-logs
./vendor/bin/behat --format=pretty --out=std --format=junit --out="../xunit-reports" "$2"
