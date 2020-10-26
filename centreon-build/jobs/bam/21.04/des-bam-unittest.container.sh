#!/bin/sh

set -x

# Get project.
PROJECT="centreon-bam-server"
FEDIR="www/modules/centreon-bam-server/react"

# Remove old reports.
rm -f /tmp/ut-be.xml
rm -f /tmp/ut-fe.xml
rm -f /tmp/coverage-be.xml
rm -f /tmp/codestyle-be.xml
rm -f /tmp/codestyle-fe.xml

# Install dependencies.
chown -R root:root "/usr/local/src/$PROJECT"
cd "/usr/local/src/$PROJECT"
composer config --global github-oauth.github.com "2cf4c72854f10e4ef54ef5dde7cd41ab474fff71"
composer install
cd "$FEDIR"
npm ci
cd ../../../..

# Run backend unit tests and code style.
./vendor/bin/phing unittest
./vendor/bin/phing codestyle

# Run frontend unit tests and code style.
cd "$FEDIR"
npm run eslint -- -o checkstyle-fe.xml -f checkstyle
npm t -- --ci --reporters=jest-junit --runInBand
cd ../../../..

# Move reports to expected places.
mv build/phpunit.xml /tmp/ut-be.xml
mv "$FEDIR/junit.xml" /tmp/ut-fe.xml
mv build/coverage-be.xml /tmp/coverage-be.xml
mv build/checkstyle-be.xml /tmp/codestyle-be.xml
mv "$FEDIR/checkstyle-fe.xml" /tmp/codestyle-fe.xml
