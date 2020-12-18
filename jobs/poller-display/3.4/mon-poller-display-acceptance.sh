#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-poller-display

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
POLLER_IMAGE="registry.centreon.com/mon-poller-display-$VERSION-$RELEASE:$DISTRIB"
CENTRAL_IMAGE="registry.centreon.com/mon-poller-display-central-$VERSION-$RELEASE:$DISTRIB"
docker pull $POLLER_IMAGE
docker pull $CENTRAL_IMAGE

# Fetch sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
get_internal_source "poller-display/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Prepare Docker Compose file.
sed -e 's#@WEB_IMAGE@#'$CENTRAL_IMAGE'#g' -e 's#@POLLER_IMAGE@#'$POLLER_IMAGE'#g' < `dirname $0`/../../../containers/poller-display/3.4/docker-compose.yml.in > mon-poller-display-dev.yml

# Prepare Behat.yml
cd "$PROJECT-$VERSION"
alreadyset=`grep log_directory: < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../acceptance-logs
mkdir ../acceptance-logs
composer install
ls features/*.feature | parallel ./vendor/bin/behat --format=pretty --out=std --format=junit --out="../xunit-reports/{/.}" "{}" || true
