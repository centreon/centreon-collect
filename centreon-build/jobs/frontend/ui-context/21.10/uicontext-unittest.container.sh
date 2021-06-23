#!/bin/sh

set -x

# Get project.
PROJECT="ui-context"

# Remove old reports.
rm -f /tmp/codestyle.xml

# Install dependencies.
chown -R root:root "/usr/local/centreon-frontend"
cd "/usr/local/centreon-frontend"
rm -rf node_modules

npm ci --legacy-peer-deps
cd "packages/ui-context"
# Run frontend unit tests and code style.

npm run eslint -- -o checkstyle.xml -f checkstyle
mv checkstyle.xml /tmp/codestyle.xml
cd "/usr/local/centreon-frontend"
