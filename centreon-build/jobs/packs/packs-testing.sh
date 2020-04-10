#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-packs

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the testing directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/internal/packs/$PROJECT-$VERSION-$RELEASE" "/srv/sources/plugin-packs/testing/packs/"

# Retrieve sources.
curl -o "$PROJECT-$VERSION.tar.gz" "http://srvi-repo.int.centreon.com/sources/plugin-packs/testing/packs/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
rm -rf "$PROJECT-$VERSION"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Upload packs to the middleware.
MIDDLEWARE='https://api.imp.centreon.com/api'
TOKEN=`curl -H "Content-Type: application/json" -X POST -d '{ "name": "batchimport", "token": "b46567488e4140d921b76cc063678af3dfeace77" }' "$MIDDLEWARE/auth/application" | python -c "import sys, json; print json.load(sys.stdin)['token']"`
cd "$PROJECT-$VERSION"
for json in *.json ; do
  echo -n '{"data": {"type": "pluginpack", "attributes": { "slug": "' > ../query.json
  slug=`python -c "import sys, json; print json.load(sys.stdin)['information']['slug']" < "$json"`
  echo -n "$slug" >> ../query.json
  echo '", "information": ' >> ../query.json
  cat < "$json" >> ../query.json
  echo '}}}' >> ../query.json
  curl -H "Content-Type: application/json" -H "centreon-imp-token: $TOKEN" -X POST -d '@-' "$MIDDLEWARE/pluginpack/pluginpack" < ../query.json
done
cd ..

# Move RPMs to the testing repository.
promote_unstable_rpms_to_testing "plugin-packs" "3.4" "el6" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "plugin-packs" "3.4" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "plugin-packs" "18.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "plugin-packs" "19.04" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "plugin-packs" "19.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "plugin-packs" "20.04" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"

# Move cache files to the testing directory.
ssh "$REPO_CREDS" mv "/srv/cache/packs/unstable/cache-$VERSION-$RELEASE" "/srv/cache/packs/testing/"
