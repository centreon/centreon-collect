#!/bin/sh

# Check arguments.
if [ -z "$PROJECT" -o -z "$COMMIT" ] ; then
  echo "You need to specify PROJECT and COMMIT environment variables."
  exit 1
fi
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# Pull build image.
BUILD_IMAGE=registry.centreon.com/sphinx
docker pull "$BUILD_IMAGE"

# Checkout sources.
rm -rf "$PROJECT"
git clone "ssh://git@github.com/centreon/$PROJECT"
cd "$PROJECT"
git checkout "$COMMIT"
cd doc

# Languages
for lang in en fr ; do
  if [ -f "$lang/conf.py" ] ; then
    containerid=`docker create $BUILD_IMAGE`
    docker cp "$lang/." "$containerid:/docs"
    docker start -a "$containerid"
    docker stop "$containerid"
    docker rm "$containerid"
  fi
done
