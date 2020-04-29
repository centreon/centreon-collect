#!/bin/sh

set -x

# Get project.
PROJECT="centreon-autodiscovery"

# Remove old reports.
rm -f /tmp/ut.xml
rm -f /tmp/coverage.xml
rm -f /tmp/codestyle.xml

# Install dependencies.
chown -R root:root "/usr/local/src/$PROJECT"
cd "/usr/local/src/$PROJECT/www/modules/centreon-autodiscovery-server/react"
npm ci

# Run frontend unit tests and code style.
npm run eslint -- -o checkstyle.xml -f checkstyle
npm t -- --ci --reporters=jest-junit --runInBand

# Move reports to expected places.
mv junit.xml /tmp/ut.xml
#mv coverage.xml /tmp/coverage.xml
mv checkstyle.xml /tmp/codestyle.xml
