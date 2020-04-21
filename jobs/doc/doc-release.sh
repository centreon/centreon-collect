#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-documentation
TARGETDIR="s3://centreon-documentation/$VERSION"
ssh $REPO_CREDS aws s3 sync --acl public-read --delete "/srv/sources/internal/doc/$PROJECT-$VERSION-$RELEASE/build/version" "$TARGETDIR"
TARGETDIR="s3://centreon-documentation/current"
ssh $REPO_CREDS aws s3 sync --acl public-read --delete "/srv/sources/internal/doc/$PROJECT-$VERSION-$RELEASE/build/current" "$TARGETDIR"
ssh $REPO_CREDS aws cloudfront create-invalidation --distribution-id E1DRHP505UQYSQ --paths "/$VERSION/"'*'
ssh $REPO_CREDS aws cloudfront create-invalidation --distribution-id E1DRHP505UQYSQ --paths '/current/*'
