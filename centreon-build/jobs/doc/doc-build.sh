#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-documentation

# Prepare for documentation build.
rm -rf build
mkdir -p build/vanilla build/testing build/unstable
BUILDIMG="$REGISTRY/node:lts"
docker pull "$BUILDIMG"
containerid=`docker create -w /$PROJECT/website "$BUILDIMG" sh -c 'yarn install && yarn run build'`
docker cp "$PROJECT" "$containerid:/"

# Build documentation in all languages.
cd "$PROJECT"
for lang in en fr ; do
  sed -e "s/@LANGUAGE@/$lang/g" < website/siteConfig.js.in > website/siteConfig.js
  docker cp website/siteConfig.js "$containerid:/$PROJECT/website/siteConfig.js"
  docker cp "$lang/sidebars.json" "$containerid:/$PROJECT/website/sidebars.json"
  docker start -a "$containerid"
  docker cp "$containerid:/$PROJECT/website/build/centreon-documentation" "../build/vanilla/$lang"
  cp -r "../build/vanilla/$lang" "../build/testing/$lang"
  find "../build/testing/$lang" -type f | xargs sed -i -e "s#@BASEURL@#$VERSION/$lang#g"
  cp -r "../build/vanilla/$lang" "../build/unstable/$lang"
  find "../build/unstable/$lang" -type f | xargs sed -i -e "s#@BASEURL@#job/centreon-documentation/job/$BRANCH_NAME/Centreon_20${lang}_20documentation_20preview#g"
done
cd ..

# Upload documentation.
put_internal_source "doc" "$PROJECT-$VERSION-$RELEASE" "build"

# Cleanup.
docker stop "$containerid"
docker rm "$containerid"
