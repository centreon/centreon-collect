#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-documentation
TARGETDIR="s3://centreon-documentation/$PROJECT/staging/$VERSION"
ssh $REPO_CREDS aws s3 rm --recursive "$TARGETDIR"
ssh $REPO_CREDS aws s3 cp --acl public-read --recursive "/srv/sources/internal/doc/$PROJECT-$VERSION-$RELEASE/build" "$TARGETDIR"
