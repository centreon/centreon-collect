#!/bin/sh

# Run unit tests.
cd /usr/local/src/centreon-license-manager/
composer install
vendor/bin/phing unittest
mv build/phpunit.xml /tmp/centreon-license-manager.xml
