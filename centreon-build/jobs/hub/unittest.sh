#!/bin/sh

set -e
set -x

# Install dependencies.
cd /usr/local/src/centreon-hub-ui
npm install

# Run unit tests.
npm run test:build
npm run lint

# Build release.
sed -i 's#http://localhost:3000/api#http://middleware:3000/api#g' config.js
npm run build:dev
