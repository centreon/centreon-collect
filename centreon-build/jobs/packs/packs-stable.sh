#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh
PACKS_SCRIPTS_PATH=$(dirname $0)

# Project.
export PROJECT=centreon-packs

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/plugin-packs/testing/packs/$PROJECT-$VERSION-$RELEASE" "/srv/sources/plugin-packs/stable/"

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "plugin-packs" "19.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "20.04" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "20.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "20.10" "el8" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "21.04" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "21.04" "el8" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "21.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "21.10" "el8" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"

# Move cache files to the stable directory.
TESTINGCACHE="/srv/cache/packs/testing/cache-$VERSION-$RELEASE"
STABLECACHE="/srv/cache/packs/stable"
$SSH_REPO 'for i in `ls '$TESTINGCACHE'` ; do rm -rf '$STABLECACHE'/$i ; mv '$TESTINGCACHE'/$i '$STABLECACHE'/ ; done ; rm -rf '$TESTINGCACHE

# Update Plugin Packs in the middleware, set not released versions to released
MIDDLEWARE='https://api.imp.centreon.com/api'
MW_KEY="b46567488e4140d921b76cc063678af3dfeace77"
MW_TOKEN=`curl -s -H "Content-Type: application/json" -X POST -d '{ "name": "batchimport", "token": "'$MW_KEY'" }' "$MIDDLEWARE/auth/application" | python -c "import sys, json; print json.load(sys.stdin)['token']"`

# get list of pp versions not yet released
IFS='
' pp_query_output=$(curl -s "$MIDDLEWARE/pluginpack/pluginpack?filter[released]=0" \
    -H "centreon-imp-token: $MW_TOKEN" | python $PACKS_SCRIPTS_PATH/get_pp_from_mw_output.py)

# one by one, set them released
for pp_info in ${pp_query_output[@]}; do
  IFS=' '
  read -a split_pp_info <<< "$pp_info"
  ppId="${split_pp_info[0]}"
  ppVersion="${split_pp_info[1]}"

  curl -s -X PATCH "$MIDDLEWARE/pluginpack/pluginpack/$ppId/version/$ppVersion" \
    -d "{\"data\":{\"id\":$ppId,\"type\":\"pluginpack\",\"attributes\":{\"released\":1}}}" \
    -H "centreon-imp-token: $MW_TOKEN" \
    -H "content-type: application/json"
done
