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
cd "/usr/local/src/$PROJECT"
composer install

# Run backend unit tests and code style.
./vendor/bin/phing unittest
./vendor/bin/phing codestyle
./vendor/bin/phing phpstan

# Move reports to expected places.
mv build/phpunit.xml /tmp/ut-be.xml
mv build/coverage-be.xml /tmp/coverage-be.xml
mv build/checkstyle-be.xml /tmp/codestyle-be.xml
mv build/phpstan.xml /tmp/phpstan.xml
