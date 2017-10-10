#!/bin/sh

set -e
set -x

# Install dependencies.
cd /usr/local/src/centreon-hub-ui
npm install

# Run unit tests.
npm run client:test:build

# Build release.
npm run client:build
