#!/bin/sh

set -x

# Get project.
PROJECT="$1"

# Remove old reports.
rm -f /tmp/ut.xml
rm -f /tmp/coverage.xml
rm -f /tmp/codestyle.xml

# Install dependencies.
cd /usr/local/src/$PROJECT/
composer install

# Run unit tests.
./vendor/bin/phing unittest

# Check code style.
./vendor/bin/phing codestyle

# Move reports to expected places.
mv build/phpunit.xml /tmp/ut.xml
mv build/coverage.xml /tmp/coverage.xml
mv build/checkstyle.xml /tmp/codestyle.xml
