#!/bin/sh

set -e
set -x

# Run unit tests.
rm -f /tmp/centreon-license-manager_ut.xml
rm -f /tmp/centreon-license-manager_coverage.xml
cd /usr/local/src/centreon-license-manager/
composer config --global github-oauth.github.com "2cf4c72854f10e4ef54ef5dde7cd41ab474fff71"
composer install
vendor/bin/phing unittest
mv build/phpunit.xml /tmp/centreon-license-manager_ut.xml
mv build/coverage.xml /tmp/centreon-license-manager_coverage.xml
