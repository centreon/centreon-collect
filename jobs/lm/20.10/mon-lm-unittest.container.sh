#!/bin/sh

set -x

# Get project.
PROJECT=centreon-license-manager

# Remove old reports.
rm -f /tmp/ut-*.xml
rm -f /tmp/coverage.xml
rm -f /tmp/codestyle-*.xml

# Install dependencies.
cd /usr/local/src/$PROJECT/
composer config --global github-oauth.github.com "2cf4c72854f10e4ef54ef5dde7cd41ab474fff71"
composer install

# Run backend unit tests and code style.
./vendor/bin/phing unittest
./vendor/bin/phing codestyle

# Move reports to expected places.
mv build/phpunit.xml /tmp/ut-be.xml
mv build/coverage.xml /tmp/coverage.xml
mv build/checkstyle.xml /tmp/codestyle-be.xml

cd "www/modules/centreon-license-manager/frontend/hooks"

# Run frontend unit tests and code style.
npm run eslint -- -o checkstyle-fe.xml -f checkstyle
npm t -- --ci --reporters=jest-junit --runInBand

# Move reports to expected places.
mv junit.xml /tmp/ut-fe.xml
mv checkstyle-fe.xml /tmp/codestyle-fe.xml
