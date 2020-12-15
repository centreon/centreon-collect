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

# Prepare build directory
mkdir -p build

# Run backend unit tests and code style.
XDEBUG_MODE=coverage composer run-script test:ci
composer run-script codestyle:ci
composer run-script phpstan:ci > build/phpstan.xml

# Move reports to expected places.
mv build/phpunit.xml /tmp/ut-be.xml
mv build/coverage.xml /tmp/coverage-be.xml
mv build/checkstyle.xml /tmp/codestyle-be.xml
mv build/phpstan.xml /tmp/phpstan.xml
