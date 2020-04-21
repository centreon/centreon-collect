#!/bin/sh

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-studio-doc

# Build API documentation.
cd "$PROJECT/api"
npm ci
npm run redoc-bundle

# Publish documentation to S3.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
put_internal_source "map-doc" "temp" docs/index.html
$SSH_REPO aws s3 cp --acl public-read "/srv/sources/internal/map-doc/temp/index.html" s3://centreon-documentation-prod/api/centreon-map/index.html
$SSH_REPO aws cloudfront create-invalidation --distribution-id E3KVGH6VYVX7DP --paths /api/centreon-map/index.html
