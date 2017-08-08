#!/bin/sh

set -x

# Run unit tests.
rm -f /tmp/centreon-middleware_ut.xml
rm -f /tmp/centreon-middleware_coverage.xml
cd /usr/local/src/centreon-imp-portal-api/
npm install
./node_modules/gulp/bin/gulp.js test:xunit

# Copy reports.
mv report.xml /tmp/centreon-middleware_ut.xml
mv build/reports/clover.xml /tmp/centreon-middleware_coverage.xml
