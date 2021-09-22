#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-awie

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
AWIE_IMAGE="$REGISTRY/mon-awie-$VERSION-$RELEASE:$DISTRIB"
docker pull $AWIE_IMAGE

# Get sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
get_internal_source "awie/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Prepare Docker Compose file.
sed 's#@WEB_IMAGE@#'$AWIE_IMAGE'#g' < `dirname $0`/../../../containers/web/20.04/docker-compose.yml.in > "$PROJECT-$VERSION/docker-compose-awie.yml"

# Run acceptance tests.
cd "$PROJECT-$VERSION"
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../acceptance-logs
mkdir ../acceptance-logs
composer install
alreadyset=`grep docker-compose-awie.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs\n      awie: docker-compose-awie.yml#g' behat.yml
fi
ls features/*.feature | parallel ./vendor/bin/behat --format=pretty --out=std --format=junit --out="../xunit-reports/{/.}" "{}" || true
