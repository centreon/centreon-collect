#!/bin/sh

set -e
set -x

# Install dependencies.
cd /usr/local/src/centreon-web
npm install

# Run unit tests.
npm run test:build
