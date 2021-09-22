#!/bin/sh

set -e

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-connector

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

MAJOR=`echo $VERSION | cut -d . -f 1,2`
EL7RPMS=`echo output/x86_64/*.el7.*.rpm`

# Publish RPMs.
if [ "$BUILD" '=' 'QA' ]
then
  put_rpms "standard" "$MAJOR" "el7" "unstable" "x86_64" "connector" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
elif [ "$BUILD" '=' 'RELEASE' ]
then
  copy_internal_source_to_testing "standard" "connector" "$PROJECT-$VERSION-$RELEASE"
  put_rpms "standard" "$MAJOR" "el7" "testing" "x86_64" "connector" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
fi
