#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
if [ -z "$PROJECT" -o -z "$COMMIT" -o -z "$DOCTAG" ] ; then
  echo "You need to specify PROJECT, COMMIT and DOCTAG environment variables."
  exit 1
fi
if [ -z "$GITREPO" ] ; then
  GITREPO="$PROJECT"
fi
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# Pull build image.
BUILD_IMAGE=registry.centreon.com/sphinx
docker pull "$BUILD_IMAGE"

# Checkout sources.
rm -rf "$GITREPO"
git clone "ssh://git@github.com/centreon/$GITREPO"
cd "$GITREPO"
git checkout "$COMMIT"
cd doc

# Languages
for lang in en fr ; do
  if [ -f "$lang/conf.py" ] ; then
    # Prepare for build.
    mkdir "$lang/templates"
    touch "$lang/templates/searchbox.html"
    cat >> "$lang/conf.py" << EOF
templates_path = ["templates"]
html_context = {
        'READTHEDOCS': True,
        'current_version': version,
        'versions': [
EOF
    sed "s/@LANG@/$lang/g" >> "$lang/conf.py" < `dirname $0`"/restversions/$PROJECT"
    cat >> "$lang/conf.py" << EOF
        ],
        'display_github': True,
        'github_user': 'centreon',
        'github_repo': '$GITREPO',
        'github_version': '$COMMIT/',
        'conf_py_path': 'doc/$lang/'
}
EOF

    # Build documentation.
    containerid=`docker create $BUILD_IMAGE`
    docker cp "$lang/." "$containerid:/docs"
    docker start -a "$containerid"
    rm -rf "html-$lang"
    docker cp "$containerid:/docs/_build/html" "html-$lang"
    docker stop "$containerid"
    docker rm "$containerid"

    # Upload documentation.
    put_internal_source "docrest" "docrest-$BUILD_NUMBER" "html-$lang"
    $SSH_REPO aws s3 sync --acl public-read "/srv/sources/internal/docrest/docrest-$BUILD_NUMBER/html-$lang" "s3://centreon-documentation-preprod/docs/$PROJECT/$lang/$DOCTAG"
  fi
done
