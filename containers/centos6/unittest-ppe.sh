#!/bin/sh

# Run unit tests.
cd /usr/local/src/centreon-web/www/modules/centreon-export
phpunit --log-junit /tmp/centreon-export.xml tests
