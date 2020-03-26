#!/bin/sh

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-studio-doc

# Build API documentation.
cd "$PROJECT/api"
npm ci
npm run bundle

# Publish documentation to S3.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
put_internal_source "map-doc" "temp" docs/index.html
$SSH_REPO aws s3 cp --acl public-read "/srv/sources/internal/map-doc/temp/index.html" s3://centreon-documentation/centreon-map/index.html
$SSH_REPO aws cloudfront create-invalidation --distribution-id E1W5L9V83QVVLX --paths /centreon-map/index.html
