#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-documentation

# Upload production sources built earlier.
tmpdir=`ssh $REPO_CREDS mktemp -d`
scp prod.tar.gz $REPO_CREDS:$tmpdir/prod.tar.gz
ssh $REPO_CREDS sh -c "'cd $tmpdir && tar xzf prod.tar.gz'"

# Upload to S3.
TARGETDIR="s3://centreon-documentation-prod/$VERSION"
ssh $REPO_CREDS aws s3 sync --acl public-read --delete "$tmpdir/prod/version" "$TARGETDIR"
ssh $REPO_CREDS rm -rf "$tmpdir"
