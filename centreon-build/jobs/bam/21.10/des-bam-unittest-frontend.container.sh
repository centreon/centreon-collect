#!/bin/sh

set -x

# Get project.
PROJECT="centreon-bam-server"
FEDIR="www/modules/centreon-bam-server/react"

# Remove old reports.
rm -f /tmp/ut-fe.xml
rm -f /tmp/codestyle-fe.xml

# Install dependencies.
chown -R root:root "/usr/local/src/$PROJECT"
cd "/usr/local/src/$PROJECT"

# Prepare build directory
mkdir -p build

cd "$FEDIR"
npm ci
cd ../../../..

# Run frontend unit tests and code style.
cd "$FEDIR"
npm run eslint -- -o checkstyle-fe.xml -f checkstyle
npm t -- --ci --reporters=jest-junit
cd ../../../..

# Move reports to expected places.
mv "$FEDIR/junit.xml" /tmp/ut-fe.xml
mv "$FEDIR/checkstyle-fe.xml" /tmp/codestyle-fe.xml
