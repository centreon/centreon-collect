#!/bin/sh

set -x

# Run unit tests.
rm -f /tmp/centreon-ppe_ut.xml
rm -f /tmp/centreon-ppe_coverage.xml
cd /usr/local/src/centreon-export/
composer config --global github-oauth.github.com "2cf4c72854f10e4ef54ef5dde7cd41ab474fff71"
composer install
vendor/bin/phing unittest
mv build/phpunit.xml /tmp/centreon-ppe_ut.xml
mv build/coverage.xml /tmp/centreon-ppe_coverage.xml
