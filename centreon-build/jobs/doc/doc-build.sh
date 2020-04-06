#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-documentation

# Reorganize directory.
rm -rf tmpsrc
mkdir -p "tmpsrc/$PROJECT"
cd "$PROJECT"
git archive HEAD | tar -C "../tmpsrc/$PROJECT" -x
cd "../tmpsrc/$PROJECT"
sed 's/@LANGUAGE@/en/g' < website/siteConfig.js.in > website/siteConfig.js
cp en/sidebars.json website/sidebars.json
mkdir website/translated_docs
mv fr website/translated_docs
cd ../..

# Create container.
BUILDIMG="$REGISTRY/node:lts"
docker pull "$BUILDIMG"
containerid=`docker create -w /$PROJECT/website "$BUILDIMG" sh -c 'yarn install && yarn run build'`
docker cp "tmpsrc/$PROJECT/." "$containerid:/$PROJECT"

# Prepare for documentation build.
rm -rf build
mkdir build

# Build documentation in all languages.
docker start -a "$containerid"
docker cp "$containerid:/$PROJECT/website/build/centreon-documentation" build/vanilla
cp -r build/vanilla build/testing
find build/testing -type f | xargs sed -i -e "s#@BASEURL@#$VERSION#g"
cp -r build/vanilla build/unstable
find build/unstable -type f | xargs sed -i -e "s#@BASEURL@#job/centreon-documentation/job/$BRANCH_NAME/Centreon_20documentation_20preview#g"

# Upload documentation.
put_internal_source "doc" "$PROJECT-$VERSION-$RELEASE" "build"

# Cleanup.
docker stop "$containerid"
docker rm "$containerid"
