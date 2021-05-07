#!/bin/sh

set -x

# Get project.
PROJECT="centreon-web"

# Remove old reports.
rm -f /tmp/ut-fe.xml
rm -f /tmp/codestyle-fe.xml

# Install dependencies.
chown -R root:root "/usr/local/src/$PROJECT"
cd "/usr/local/src/$PROJECT"

# Run frontend unit tests and code style.
npm run eslint -- -o checkstyle-fe.xml -f checkstyle
npm t -- --ci --reporters=jest-junit --runInBand

# Move reports to expected places.
mv junit.xml /tmp/ut-fe.xml
mv checkstyle-fe.xml /tmp/codestyle-fe.xml
