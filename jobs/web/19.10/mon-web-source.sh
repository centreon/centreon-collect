#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

#
# This script will generate Centreon Web sources from the local clone
# of Centreon Web repository (centreon-web directory). These sources
# will then be pushed to the internal repository (srvi-repo) and used
# in downstream jobs, thanks to the property file generated at the end
# of the script.
#

# Project.
export PROJECT=centreon-web
tar czf "$PROJECT-git.tar.gz" "$PROJECT"

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
git rm .gitattributes
git archive --worktree-attributes --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
git reset --hard HEAD
cd ..

# Create and populate container.
BUILD_IMAGE="registry.centreon.com/mon-build-dependencies-19.10:centos7"
docker pull "$BUILD_IMAGE"
containerid=`docker create -e "PROJECT=$PROJECT" -e "VERSION=$VERSION" -e "COMMIT=$COMMIT" $BUILD_IMAGE /usr/local/bin/source.sh`
docker cp `dirname $0`/mon-web-source.container.sh "$containerid:/usr/local/bin/source.sh"
docker cp "$PROJECT-$VERSION.tar.gz" "$containerid:/usr/local/src/"

# Run container that will generate complete tarball.
docker start -a "$containerid"
rm -f "$PROJECT-$VERSION.tar.gz" "vendor.tar.gz"
docker cp "$containerid:/usr/local/src/$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION.tar.gz"
docker cp "$containerid:/usr/local/src/vendor.tar.gz" "vendor.tar.gz"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

# Send it to srvi-repo.
put_internal_source "web" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
put_internal_source "web" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"
put_internal_source "web" "$PROJECT-$VERSION-$RELEASE" "vendor.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
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
