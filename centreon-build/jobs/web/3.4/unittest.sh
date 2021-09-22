#!/bin/sh

set -e
set -x

# Install dependencies.
cd /usr/local/src/centreon
npm install

# Run unit tests.
npm run test:build
