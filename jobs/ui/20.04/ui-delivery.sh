#!/bin/bash

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-ui

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  TARGETDIR="s3://centreon-documentation/$PROJECT/20.04/staging"
  ssh $REPO_CREDS aws s3 rm --recursive "$TARGETDIR"
  ssh $REPO_CREDS aws s3 cp --acl public-read --recursive "/srv/sources/internal/ui/$PROJECT-$VERSION-$RELEASE/storybook" "$TARGETDIR"

#
# CI delivery.
#
elif [ "$BUILD" '=' 'REFERENCE' ] ; then
  TARGETDIR="s3://centreon-documentation/$PROJECT/20.04/unstable"
  ssh $REPO_CREDS aws s3 rm --recursive "$TARGETDIR"
  ssh $REPO_CREDS aws s3 cp --acl public-read --recursive "/srv/sources/internal/ui/$PROJECT-$VERSION-$RELEASE/storybook" "$TARGETDIR"
fi
