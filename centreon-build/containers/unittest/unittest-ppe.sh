#!/bin/sh

set -x

# Remove old reports.
rm -f /tmp/centreon-ppe_ut.xml
rm -f /tmp/centreon-ppe_coverage.xml
rm -f /tmp/centreon-ppe_codestyle.xml

# Install dependencies.
cd /usr/local/src/centreon-export/
composer config --global github-oauth.github.com "2cf4c72854f10e4ef54ef5dde7cd41ab474fff71"
composer install

# Run unit tests.
vendor/bin/phing unittest

# Check code style.
vendor/bin/phing codestyle

# Move reports to expected places.
mv build/phpunit.xml /tmp/centreon-ppe_ut.xml
mv build/coverage.xml /tmp/centreon-ppe_coverage.xml
mv build/checkstyle.xml /tmp/centreon-ppe_codestyle.xml
