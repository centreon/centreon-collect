#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "map" "21.10" "el7" "noarch" "map-web" "$PROJECT-web-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "map" "21.10" "el8" "noarch" "map-web" "$PROJECT-web-$VERSION-$RELEASE"
