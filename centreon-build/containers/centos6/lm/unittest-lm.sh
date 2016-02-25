#!/bin/sh

# Run unit tests.
cd /usr/local/src/centreon-license-manager/
composer install
vendor/bin/phing -logfile /tmp/centreon-license-manager.xml unittest
