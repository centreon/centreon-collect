#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-documentation

# Prepare for documentation build.
rm -rf build preview
mkdir -p build/vanilla build/version build/current preview
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
  docker cp "$containerid:/$PROJECT/website/build/centreon-documentation" "build/vanilla/$lang"
  cp -r "build/vanilla/$lang" "build/version/$lang"
  find "build/version/$lang" -type f | xargs -d '\n' sed -i -e "s#@BASEURL@#$VERSION/$lang#g"
  cp -r "build/vanilla/$lang" "build/current/$lang"
  find "build/current/$lang" -type f | xargs -d '\n' sed -i -e "s#@BASEURL@#current/$lang#g"
  cp -r "build/vanilla/$lang" "preview/$lang"
  find "preview/$lang" -type f | xargs -d '\n' sed -i -e "s#@BASEURL@#job/centreon-documentation/job/$BRANCH_NAME/Centreon_20documentation_20preview/${lang}#g"
  docker stop "$containerid"
  docker rm "$containerid"
done

# Upload documentation.
cp `dirname $0`/redirect.html build/vanilla/index.html
cd build
tar czf vanilla.tar.gz vanilla
rm -rf vanilla
cd ..
cp `dirname $0`/redirect.html build/version/index.html
cp `dirname $0`/redirect.html build/current/index.html
cp `dirname $0`/redirect.html preview/index.html
put_internal_source "doc" "$PROJECT-$VERSION-$RELEASE" "build"
