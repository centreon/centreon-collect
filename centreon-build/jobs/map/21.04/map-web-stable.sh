#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map

# Move RPMs to the stable repository.
promote_rpms_from_testing_to_stable "map" "21.04" "el7" "noarch" "map-web" "$PROJECT-web-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "map" "21.04" "el8" "noarch" "map-web" "$PROJECT-web-$VERSION-$RELEASE"
