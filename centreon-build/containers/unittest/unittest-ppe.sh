#!/bin/sh

set -e
set -x

# Run unit tests.
rm -f /tmp/centreon-export.xml
cd /usr/local/src/centreon-export
phpunit --bootstrap www/modules/centreon-export/tests/bootstrap.php --log-junit /tmp/centreon-export.xml tests
