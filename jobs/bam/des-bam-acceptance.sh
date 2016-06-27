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
BAM_IMAGE=ci.int.centreon.com:5000/des-bam:$DISTRIB
docker pull $WEBDRIVER_IMAGE
docker pull $BAM_IMAGE

# Prepare Docker Compose file.
cd centreon-bam
sed 's#@WEB_IMAGE@#'$BAM_IMAGE'#g' < `dirname $0`/../../containers/web/docker-compose.yml.in > docker-compose-bam.yml

# Prepare Behat.yml
alreadyset=`grep docker-compose-bam.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      bam: docker-compose-bam.yml#g' behat.yml
fi

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
#composer install
#composer update
#ls features/*.feature | parallel /opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}"
