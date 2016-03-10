#!/bin/sh

set -e
set -x

# Run unit tests.
rm -f /tmp/centreon-export.xml
cd /usr/local/src/centreon-export/www/modules/centreon-export/tests
phpunit --bootstrap bootstrap.php --log-junit /tmp/centreon-export.xml .
