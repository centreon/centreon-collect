#!/bin/sh

set -x

# Get project.
PROJECT=centreon-pp-manager

# Remove old reports.
rm -f /tmp/ut-be.xml
rm -f /tmp/coverage-be.xml
rm -f /tmp/codestyle-be.xml
rm -f /tmp/phpstan.xml

# Install dependencies.
chown -R root:root "/usr/local/src/$PROJECT"
cd "/usr/local/src/$PROJECT"
composer config --global github-oauth.github.com $GITHUB_TOKEN
composer install

# Prepare build directory
mkdir -p build

# Run backend unit tests and code style.
XDEBUG_MODE=coverage composer run-script test:ci
composer run-script codestyle:ci
composer run-script phpstan:ci > build/phpstan.xml

# Move reports to expected places.
mv build/phpunit.xml /tmp/ut-be.xml
mv build/coverage-be.xml /tmp/coverage-be.xml
mv build/checkstyle-be.xml /tmp/codestyle-be.xml
mv build/phpstan.xml /tmp/phpstan.xml
