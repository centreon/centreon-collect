#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-poller-display

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
WEBDRIVER_IMAGE=ci.int.centreon.com:5000/mon-phantomjs:latest
POLLER_IMAGE="ci.int.centreon.com:5000/mon-poller-display-$VERSION-$RELEASE:$DISTRIB"
CENTRAL_IMAGE="ci.int.centreon.com:5000/mon-poller-display-central-$VERSION-$RELEASE:$DISTRIB"
docker pull $WEBDRIVER_IMAGE
docker pull $POLLER_IMAGE
docker pull $CENTRAL_IMAGE

# Fetch sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
wget "http://srvi-repo.int.centreon.com/sources/internal/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"
cd "$PROJECT-$VERSION"

# Prepare Docker Compose file.
sed -e 's#@WEB_IMAGE@#'$CENTRAL_IMAGE'#g' -e 's#@POLLER_IMAGE@#'$POLLER_IMAGE'#g' < `dirname $0`/../../containers/poller-display/docker-compose.yml.in > mon-poller-display-dev.yml

# Prepare Behat.yml
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
composer update
ls features/*.feature | parallel ./vendor/bin/behat --format=junit --out="../xunit-reports/{/.}" "{}" || true
for file in `find ../xunit-reports -name '*.xml'` ; do
  sed -i "s/<testsuite /<testsuite package=\"acceptance.$DISTRIB\" /g" $file
done
