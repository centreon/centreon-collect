#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-clib

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "standard" "clib" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "20.04" "el7" "x86_64" "clib" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "20.04" "el8" "x86_64" "clib" "$PROJECT-$VERSION-$RELEASE"
  SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-clib -V latest -p'"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "standard" "20.04" "el7" "x86_64" "clib" "$PROJECT-$VERSION-$RELEASE"
  promote_canary_rpms_to_unstable "standard" "20.04" "el8" "x86_64" "clib" "$PROJECT-$VERSION-$RELEASE"
fi
