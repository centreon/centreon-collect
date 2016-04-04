#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Pull mon-lm image.
docker pull ci.int.centreon.com:5000/mon-lm:centos$CENTOS_VERSION

# Check that phantomjs is running.
export PHANTOMJS_RUNNING=1
nc -w 0 localhost 4444 || export PHANTOMJS_RUNNING=0 || true
if [ "$PHANTOMJS_RUNNING" -ne 1 ] ; then
  screen -d -m phantomjs --webdriver=4444
fi

# Run acceptance tests.
export CENTREON_WEB_IMAGE=ci.int.centreon.com:5000/mon-web:centos$CENTOS_VERSION
export CENTREON_PPE_IMAGE=ci.int.centreon.com:5000/mon-lm:centos$CENTOS_VERSION
rm -rf xunit-reports
mkdir xunit-reports
cd centreon-license-manager
alreadyset=`grep ci.int.centreon.com < composer.json || true`
if [ -z "$alreadyset" ] ; then
  sed -i 's#    "require-dev": {#    "repositories": [\n      { "type": "composer", "url": "http://ci.int.centreon.com" }\n    ],\n    "config": {\n        "secure-http": false\n    },\n    "require-dev": {#g' composer.json
fi
composer install
composer update
ls features/*.feature | parallel /opt/behat/vendor/bin/behat --strict --format=junit --out="../xunit-reports/{/.}" "{}"
