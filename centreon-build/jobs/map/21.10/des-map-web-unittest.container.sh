#!/bin/sh

set -x

# Get project.
PROJECT="centreon-map"

# Remove old reports.
rm -f /tmp/codestyle-be.xml

# Install dependencies.
chown -R root:root "/usr/local/src/$PROJECT"
cd "/usr/local/src/$PROJECT"
composer install

# Prepare build directory
mkdir -p build

# Run backend code style.
composer run-script codestyle:ci

# Move reports to expected places.
mv build/checkstyle.xml /tmp/codestyle-be.xml
