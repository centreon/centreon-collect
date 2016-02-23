#!/bin/sh

# Get version.
VERSION=
VERSION_NUM=0
VERSION_EXTRA=
for file in centreon-export/www/modules/centreon-export/UPGRADE/* ; do
  full_version=`echo "$file" | cut -d - -f 5`
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
  elif [ \( \( "$current_num" -eq "$VERSION_NUM" \) ] ; then
    if [ \( \( \! -z "$VERSION_EXTRA" \) -a \( "$extra" '>' "$VERSION_EXTRA" \) \) -o \( -z "$extra" \) ] ; then
      VERSION_EXTRA="$extra"
    fi
  fi
done
export VERSION="$VERSION"
export VERSION_EXTRA="$VERSION_EXTRA"

# Generate archive of Centreon PPE.
cd centreon-export
git archive --prefix="centreon-export-$VERSION/" "$GIT_BRANCH" | gzip > "../centreon-export-$VERSION.tar.gz"
cd ..

# Copy files to server.
FILES="centreon-export-$VERSION.tar.gz"
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.merethis.net:/tmp/"
