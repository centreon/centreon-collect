#!/bin/sh

set -e
set -x

# Run unit tests.
rm -f /tmp/centreon-ppe_ut.xml
rm -f /tmp/centreon-ppe_coverage.xml
cd /usr/local/src/centreon-export/www/modules/centreon-export/tests
phpunit --bootstrap bootstrap.php --log-junit /tmp/centreon-ppe_ut.xml --coverage-clover /tmp/centreon-ppe_coverage.xml .
