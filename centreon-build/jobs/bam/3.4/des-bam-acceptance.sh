#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-bam-server

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
REGISTRY="ci.int.centreon.com:5000"
WEBDRIVER_IMAGE=selenium/standalone-chrome:latest
BAM_IMAGE="$REGISTRY/des-bam-$VERSION-$RELEASE:$DISTRIB"
docker pull $WEBDRIVER_IMAGE
docker pull $BAM_IMAGE

# Get sources.
rm -rf "$PROJECT-$VERSION-full" "$PROJECT-$VERSION-full.tar.gz"
get_internal_source "bam/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-full.tar.gz"
tar xzf "$PROJECT-$VERSION-full.tar.gz"
cd "$PROJECT-$VERSION-full"

# Prepare Docker Compose file.
sed 's#@WEB_IMAGE@#'$BAM_IMAGE'#g' < `dirname $0`/../../../containers/web/3.4/docker-compose.yml.in > docker-compose-bam.yml

# Prepare Behat.yml
alreadyset=`grep docker-compose-bam.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      log_directory: ../acceptance-logs-wip\n      bam: docker-compose-bam.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
rm -rf ../acceptance-logs
mkdir ../acceptance-logs
composer install
composer update
ls features/*.feature | parallel ./vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}"
