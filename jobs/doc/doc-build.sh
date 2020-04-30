#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-documentation

# Prepare for documentation build.
rm -rf vanilla
mkdir -p vanilla
BUILDIMG="$REGISTRY/node:lts"
docker pull "$BUILDIMG"

# Build documentation in all languages.
for lang in en fr ; do
  # Extract sources in lang-specific directory.
  rm -rf "$lang"
  mkdir "$lang"
  cd "$PROJECT"
  git archive HEAD | tar -C "../$lang" -x
  cd ..

  # Update sources for target language.
  cd "$lang"
  sed -e "s/@LANGUAGE@/$lang/g" < website/siteConfig.js.in > website/siteConfig.js
  mv "$lang/sidebars.json" "website/sidebars.json"
  mv "website/pages/$lang" "pages-$lang"
  rm -rf website/pages
  mkdir website/pages
  mv "pages-$lang" website/pages/en
  mv "website/core/$lang" "core-$lang"
  rm -rf website/core
  mv "core-$lang" website/core
  cd ..

  # Build documentation.
  containerid=`docker create -w /$PROJECT/website "$BUILDIMG" sh -c 'npm ci && npm run build'`
  docker cp "$lang/." "$containerid:/$PROJECT"
  docker start -a "$containerid"
  docker cp "$containerid:/$PROJECT/website/build/centreon-documentation" "vanilla/$lang"
  find "vanilla/$lang" -type f | xargs -d '\n' sed -i -e "s#@BASEURL@#@BASEURL@/${lang}#g"
  docker stop "$containerid"
  docker rm "$containerid"
done
cp `dirname $0`/redirect.html vanilla/index.html

# Build preview documentation.
rm -rf preview
cp -r vanilla preview
find preview -type f | xargs -d '\n' sed -i -e "s#@BASEURL@#job/centreon-documentation/job/$BRANCH_NAME/Centreon_20documentation_20preview#g"

# Prepare vanilla tarball for stashing.
tar czf vanilla.tar.gz vanilla
