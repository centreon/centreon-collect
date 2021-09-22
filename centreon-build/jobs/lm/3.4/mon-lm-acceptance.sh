#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-license-manager

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
REGISTRY="registry.centreon.com"
LM_IMAGE="$REGISTRY/mon-lm-$VERSION-$RELEASE:$DISTRIB"
MIDDLEWARE_IMAGE="$REGISTRY/mon-middleware:latest"
REDIS_IMAGE="redis:latest"
OPENLDAP_IMAGE="$REGISTRY/mon-openldap:latest"
SQUID_SIMPLE_IMAGE="$REGISTRY/mon-squid-simple:latest"
SQUID_BASIC_AUTH_IMAGE="$REGISTRY/mon-squid-basic-auth:latest"
docker pull $LM_IMAGE
docker pull $MIDDLEWARE_IMAGE
docker pull $REDIS_IMAGE
docker pull $OPENLDAP_IMAGE
docker pull $SQUID_SIMPLE_IMAGE
docker pull $SQUID_BASIC_AUTH_IMAGE

# Get sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
get_internal_source "lm/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Prepare Docker Compose file.
sed -e 's#@WEB_IMAGE@#'$LM_IMAGE'#g' -e 's#@MIDDLEWARE_IMAGE@#'$MIDDLEWARE_IMAGE'#g' < `dirname $0`/../../../containers/middleware/docker-compose-web.yml.in > "$PROJECT-$VERSION/docker-compose-lm.yml"
sed -e 's#@WEB_IMAGE@#'$LM_IMAGE'#g' -e 's#@MIDDLEWARE_IMAGE@#'$MIDDLEWARE_IMAGE'#g' < `dirname $0`/../../../containers/squid/simple/docker-compose-middleware.yml.in > "$PROJECT-$VERSION/docker-compose-lm-squid-simple.yml"
sed -e 's#@WEB_IMAGE@#'$LM_IMAGE'#g' -e 's#@MIDDLEWARE_IMAGE@#'$MIDDLEWARE_IMAGE'#g' < `dirname $0`/../../../containers/squid/basic-auth/docker-compose-middleware.yml.in > "$PROJECT-$VERSION/docker-compose-lm-squid-basic-auth.yml"

# Prepare behat.yml.
cd "$PROJECT-$VERSION"
alreadyset=`grep docker-compose-lm.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs\n      lm: docker-compose-lm.yml\n      lm_squid_simple: docker-compose-lm-squid-simple.yml\n      lm_squid_basic_auth: docker-compose-lm-squid-basic-auth.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../acceptance-logs
mkdir ../acceptance-logs
composer install
ls features/*.feature | parallel -j 1 ./vendor/bin/behat --format=pretty --out=std --format=junit --out="../xunit-reports/{/.}" "{}" || true
