#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-documentation
TARGETDIR="s3://centreon-documentation/$VERSION"
ssh $REPO_CREDS aws s3 sync --acl public-read --delete "/srv/sources/internal/doc/$PROJECT-$VERSION-$RELEASE/build/version" "$TARGETDIR"
ssh $REPO_CREDS aws cloudfront create-invalidation --distribution-id E3KVGH6VYVX7DP --paths "/$VERSION/"'*'
TARGETDIR="s3://centreon-documentation/current"
ssh $REPO_CREDS aws s3 sync --acl public-read --delete "/srv/sources/internal/doc/$PROJECT-$VERSION-$RELEASE/build/current" "$TARGETDIR"
ssh $REPO_CREDS aws cloudfront create-invalidation --distribution-id E3KVGH6VYVX7DP --paths '/current/*'
