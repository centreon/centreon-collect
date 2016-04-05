#!/bin/sh

set -e
set -x

# Run unit tests.
rm -f /tmp/centreon-middleware_ut.xml
rm -f /tmp/centreon-middleware_coverage.xml
cd /usr/local/src/centreon-imp-portal-api/
npm install gulp
gulp test:xunit

# Copy reports.
mv xunit.xml /tmp/centreon-middleware_ut.xml
mv build/reports/coverage.xml /tmp/centreon-middleware_coverage.xml
