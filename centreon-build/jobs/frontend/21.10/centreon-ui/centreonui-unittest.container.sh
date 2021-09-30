#!/bin/sh

set -x

# Get project.
PROJECT="centreon-ui"

# Remove old reports.
rm -f /tmp/ut.xml
rm -f /tmp/codestyle.xml

# Install dependencies.
chown -R root:root "/usr/local/centreon-frontend"

cd "/usr/local/centreon-frontend"
npm ci --legacy-peer-deps

cd "packages/centreon-ui"
npm ci --legacy-peer-deps

# Run frontend unit tests and code style.
npm run eslint -- -o checkstyle.xml -f checkstyle
npm run build:storybook
npm t -- --ci --reporters=jest-junit
# Move reports to expected places.
mv junit.xml /tmp/ut.xml
mv checkstyle.xml /tmp/codestyle.xml
cd "/usr/local/centreon-frontend"
