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
WEB_FRESH_IMAGE=ci.int.centreon.com:5000/mon-web-fresh:$DISTRIB
docker pull $WEB_IMAGE
docker pull $WEB_FRESH_IMAGE

# Check that phantomjs is running.
export PHANTOMJS_RUNNING=1
nc -w 0 localhost 4444 || export PHANTOMJS_RUNNING=0 || true
if [ "$PHANTOMJS_RUNNING" -ne 1 ] ; then
  screen -d -m phantomjs --webdriver=4444
fi

# Run acceptance tests.
rm -rf xunit-reports
mkdir xunit-reports
cd centreon-web
alreadyset=`grep ci.int.centreon.com < composer.json || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    "require-dev": {#    "repositories": [\n      { "type": "composer", "url": "http://ci.int.centreon.com" }\n    ],\n    "config": {\n        "secure-http": false\n    },\n    "require-dev": {#g' composer.json
fi
composer install
composer update
alreadyset=`grep ci.int.centreon.com < behat.yml || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension#    Centreon\\Test\\Behat\\Extensions\\ContainerExtension:\n      images:\n        web: '$WEB_IMAGE'\n        web_fresh: '$WEB_FRESH_IMAGE'#g' behat.yml
fi
ls features/*.feature | parallel /opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}"
