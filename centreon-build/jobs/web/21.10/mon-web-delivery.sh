#!/bin/sh

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

MAJOR=`echo $VERSION | cut -d . -f 1,2`
EL7RPMS=`echo output/noarch/*.el7.*.rpm`
EL8RPMS=`echo output/noarch/*.el8.*.rpm`

# Publish RPMs.
if [ "$BUILD" '=' 'QA' ]
then
  put_internal_source "web" "$PROJECT-$VERSION-$RELEASE" centreon-api-v21.10.html
  put_rpms "standard" "$MAJOR" "el7" "unstable" "noarch" "web" "centreon-web-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "unstable" "noarch" "web" "centreon-web-$VERSION-$RELEASE" $EL8RPMS
elif [ "$BUILD" '=' 'RELEASE' ]
then
  put_testing_source "standard" "web" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz" centreon-api-v21.10.html
  SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
  $SSH_REPO aws s3 cp --acl public-read "/srv/sources/internal/web/centreon-web-$VERSION-$RELEASE/centreon-api-v21.10.html" s3://centreon-documentation-prod/api/centreon-web/index.html
  $SSH_REPO aws cloudfront create-invalidation --distribution-id E3KVGH6VYVX7DP --paths /api/centreon-web/index.html
  put_rpms "standard" "$MAJOR" "el7" "testing" "noarch" "web" "centreon-web-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "testing" "noarch" "web" "centreon-web-$VERSION-$RELEASE" $EL8RPMS
elif [ "$BUILD" '=' 'CI' ]
then
  put_rpms "standard" "$MAJOR" "el7" "canary" "noarch" "web" "centreon-web-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "canary" "noarch" "web" "centreon-web-$VERSION-$RELEASE" $EL8RPMS
fi
