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
WEB_IMAGE=ci.int.centreon.com:5000/mon-web:$DISTRIB
PPE_IMAGE=ci.int.centreon.com:5000/mon-ppe:$DISTRIB
PPE1_IMAGE=ci.int.centreon.com:5000/mon-ppe1:$DISTRIB
docker pull $WEB_IMAGE
docker pull $PPE_IMAGE
docker pull $PPE1_IMAGE

# Check that phantomjs is running.
export PHANTOMJS_RUNNING=1
nc -w 0 localhost 4444 || export PHANTOMJS_RUNNING=0 || true
if [ "$PHANTOMJS_RUNNING" -ne 1 ] ; then
  screen -d -m phantomjs --webdriver=4444
fi

# Prepare Docker Compose file.
cd centreon-export
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g' < `dirname $0`/../../containers/web/docker-compose.yml.in > docker-compose-web.yml
sed 's#@WEB_IMAGE@#'$PPE_IMAGE'#g' < `dirname $0`/../../containers/web/docker-compose.yml.in > docker-compose-ppe.yml
sed 's#@WEB_IMAGE@#'$PPE1_IMAGE'#g' < `dirname $0`/../../containers/web/docker-compose.yml.in > docker-compose-ppe1.yml

# Run acceptance tests.
rm -rf ../xunit-reports
mkdir ../xunit-reports
composer install
composer update
alreadyset=`grep docker-compose-ppe.yml < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      web: docker-compose-web.yml\n      ppe: docker-compose-ppe.yml\n      ppe1: docker-compose-ppe1.yml#g' behat.yml
fi
ls features/*.feature | parallel /opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}"
