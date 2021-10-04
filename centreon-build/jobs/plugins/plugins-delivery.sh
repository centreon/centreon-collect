#!/bin/sh

set -e

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-plugins

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

MAJOR=`echo $VERSION | cut -d . -f 1,2`
EL7RPMS=`echo output/noarch/*.el7.*.rpm`
EL8RPMS=`echo output/noarch/*.el8.*.rpm`

# Publish RPMs to unstable repo
put_rpms "standard" "$MAJOR" "el7" "unstable" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
put_rpms "standard" "$MAJOR" "el8" "unstable" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
