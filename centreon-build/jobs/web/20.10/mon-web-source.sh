#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Get version.
VERSION=
VERSION_NUM=0
VERSION_EXTRA=
for file in centreon-web/www/install/php/Update-*.php ; do
  full_version=`echo "$file" | cut -d - -f 3- | sed -e 's/.post.php$//' -e 's/.php$//'`
  major=`echo "$full_version" | cut -d . -f 1`
  minor=`echo "$full_version" | cut -d . -f 2`
  # Patch is not necessarily set.
  patch=`echo "$full_version" | cut -d . -f 3 | cut -d - -f 1`
  if [ -z "$patch" ] ; then
    patch=0
  fi
  extra=`echo "$full_version" | grep - | cut -d - -f 2`
  current_num=$(($major*10000+$minor*100+$patch))
  # If version number is greater than current version, directly set variables.
  if [ \( "$current_num" -gt "$VERSION_NUM" \) ] ; then
    VERSION="$major.$minor.$patch"
    VERSION_NUM="$current_num"
    VERSION_EXTRA="$extra"
  # If version numbers are equal, the empty extra has priority.
  # Otherwise the 'greater' extra is prefered.
  elif [ \( "$current_num" -eq "$VERSION_NUM" \) ] ; then
    if [ \( \( \! -z "$VERSION_EXTRA" \) -a \( "$extra" '>' "$VERSION_EXTRA" \) \) -o \( -z "$extra" \) ] ; then
      VERSION_EXTRA="$extra"
    fi
  fi
done
if [ -z "$VERSION_EXTRA" ] ; then
  export VERSION="$VERSION"
else
  export VERSION="$VERSION-$VERSION_EXTRA"
fi

# Get release.
cd centreon-web
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
if [ "$BUILD" '=' 'RELEASE' ] ; then
  export RELEASE="$BUILD_NUMBER"
else
  export RELEASE="$now.$COMMIT"
fi

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Prepare base source tarball.
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
cd ..

# Create and populate container.
BUILD_IMAGE="registry.centreon.com/mon-build-dependencies-20.10:centos7"
docker pull "$BUILD_IMAGE"
containerid=`docker create -e "PROJECT=$PROJECT" -e "VERSION=$VERSION" -e "COMMIT=$COMMIT" $BUILD_IMAGE /usr/local/bin/source.sh`
docker cp `dirname $0`/mon-web-source.container.sh "$containerid:/usr/local/bin/source.sh"
docker cp "$PROJECT-$VERSION.tar.gz" "$containerid:/usr/local/src/"

# Run container that will generate complete tarball.
docker start -a "$containerid"
rm -f "$PROJECT-$VERSION.tar.gz" "vendor.tar.gz"
docker cp "$containerid:/usr/local/src/$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION.tar.gz"
docker cp "$containerid:/usr/local/src/vendor.tar.gz" "vendor.tar.gz"
docker cp "$containerid:/usr/local/src/centreon-api-v2.html" centreon-api-v2.html

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF

# Generate summary report.
rm -rf summary
cp -r `dirname $0`/../../common/build-artifacts summary
cp `dirname $0`/jobData.json summary/
generate_summary
