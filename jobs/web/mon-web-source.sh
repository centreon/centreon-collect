#!/bin/sh

set -e
set -x

#
# This script will generate Centreon Web sources from the local clone
# of Centreon Web repository (centreon-web directory) and Centreon Plugins
# (centreon-plugins directory). These sources will then be pushed to the
# internal repository (srvi-repo) and used in downstream jobs, thanks to
# the property file generated at the end of the script.
#

# Get version.
VERSION=
VERSION_NUM=0
VERSION_EXTRA=
for file in centreon-web/www/install/sql/centreon/*.sql ; do
  full_version=`echo "$file" | cut -d _ -f 3 | sed 's/.sql$//'`
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
export VERSION="$VERSION"
export VERSION_EXTRA="$VERSION_EXTRA"

# Get release.
cd centreon-web
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
# We do not care about $VERSION_EXTRA, as $now is strictly incremental
# and officially released packages do not always care about semantic
# versioning.
export RELEASE="$now.$COMMIT"

# Prepare source directory
rm -rf "../centreon-$VERSION"
mkdir "../centreon-$VERSION"
git archive "$GIT_BRANCH" | tar -C "../centreon-$VERSION" -x
cd ../centreon-plugins
# We should use "$GIT_BRANCH" instead of 2.7.x. However nothing seems to work as expected.
git archive --prefix=plugins/ "origin/2.7.x" | tar -C "../centreon-$VERSION" -x

# Generate release notes.
# Code adapted from centreon-tools/make_package.sh.
cd "../centreon-$VERSION/doc/en"
make SPHINXOPTS="-D html_theme=scrolls" html
cp "_build/html/release_notes/centreon-$major.$minor/centreon-$VERSION.html" "../../www/install/RELEASENOTES.html"
sed -i \
    -e "/<link/d" \
    -e "/<script .*>.*<\/script>/d" \
    -e "s/href=\"..\//href=\"http:\/\/documentation.centreon.com\/docs\/centreon\/en\/latest\//g" \
    -e "/<\/head>/i \
    <style type=\"text/css\">\n \
    #toc, .footer, .relnav, .header { display: none; }\n \
    <\/style>" ../../www/install/RELEASENOTES.html
make clean
cd ../../..

# Create source tarball.
tar czf "centreon-$VERSION.tar.gz" "centreon-$VERSION"

# Send it to srvi-repo.
FILES="centreon-$VERSION.tar.gz"
DEST="/srv/sources/internal/centreon-web-$VERSION-$RELEASE"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "$DEST"
scp -o StrictHostKeyChecking=no $FILES "ubuntu@srvi-repo.int.centreon.com:$DEST"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=centreon-web
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
EOF
