#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 2 ] ; then
  echo "USAGE: $0 <centos7|centos8> [feature]"
  exit 1
fi
DISTRIB="$1"

# Pull images.
WEB_IMAGE="$REGISTRY/mon-web-$VERSION-$RELEASE:$DISTRIB"
docker pull $WEB_IMAGE

# Fetch sources.
rm -rf "$PROJECT-$VERSION"
tar xzf "$PROJECT-$VERSION.tar.gz"
cd "$PROJECT-$VERSION"
rm -rf vendor
tar xzf "../vendor.tar.gz"
cd ..

# Prepare Docker Compose file.
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../../containers/web/21.10/docker-compose-api.yml.in > "$PROJECT-$VERSION/docker-compose-web.yml"

# Prepare Behat.yml
cd "$PROJECT-$VERSION"
alreadyset=`grep docker-compose-web.yml < tests/api/behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../api-integration-test-logs\n      web: docker-compose-web.yml#g' tests/api/behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../api-integration-test-logs
mkdir ../api-integration-test-logs
./vendor/bin/behat --config tests/api/behat.yml --format=pretty --out=std --format=junit --out="../xunit-reports" "$2"
