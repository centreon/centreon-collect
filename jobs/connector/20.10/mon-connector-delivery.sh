#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-connector

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Publish RPMs.
if [ "$BUILD" '=' 'QA' ]
then
  put_rpms "standard" "$MAJOR" "el7" "unstable" "x86_64" "connector" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "unstable" "x86_64" "connector" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS
elif [ "$BUILD" '=' 'RELEASE' ]
then
  copy_internal_source_to_testing "standard" "connector" "$PROJECT-$VERSION-$RELEASE"
  put_rpms "standard" "$MAJOR" "el7" "testing" "x86_64" "connector" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "testing" "x86_64" "connector" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS
fi
