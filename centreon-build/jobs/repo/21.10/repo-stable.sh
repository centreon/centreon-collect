#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

for distrib in el7 el8 ; do
  for repo in standard business plugin-packs ; do
    promote_testing_rpms_to_stable "$repo" "21.10" "$distrib" "noarch" "repo" ""
  done
done
