#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-documentation

# We will prepare two source directories ("$VERSION/" and "current/").
rm -rf prod vanilla
mkdir prod
tar xzf vanilla.tar.gz
cp -r vanilla prod/version
mv vanilla prod/current

# Let's replace their respective base URL.
find "prod/version" -type f | xargs -d '\n' sed -i -e "s#@BASEURL@#$VERSION#g"
find "prod/current" -type f | xargs -d '\n' sed -i -e "s#@BASEURL@#current#g"
tar czf prod.tar.gz prod

# We cannot upload sources to S3 directly. We need to get
# through the repository server so upload sources to it first.
tmpdir=`ssh $REPO_CREDS mktemp -d`
scp prod.tar.gz $REPO_CREDS:$tmpdir/prod.tar.gz
ssh $REPO_CREDS sh -c "'cd $tmpdir && tar xzf prod.tar.gz'"

# Upload to S3.
TARGETDIR="s3://centreon-documentation-preprod/$VERSION"
ssh $REPO_CREDS aws s3 sync --acl public-read --delete "$tmpdir/prod/version" "$TARGETDIR"
TARGETDIR="s3://centreon-documentation-preprod/current"
#ssh $REPO_CREDS aws s3 sync --acl public-read --delete "$tmpdir/prod/current" "$TARGETDIR"
ssh $REPO_CREDS aws cloudfront create-invalidation --distribution-id E1DRHP505UQYSQ --paths "/$VERSION/"'*'
ssh $REPO_CREDS rm -rf "$tmpdir"
