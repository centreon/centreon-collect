#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Pull mon-build-dependencies container.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos$CENTOS_VERSION

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get Centreon Web sources.
if [ \! -d centreon-web ] ; then
  git clone https://github.com/centreon/centreon centreon-web
fi

# Get Centreon Plugins sources.
if [ \! -d centreon-plugins ] ; then
  git clone https://github.com/centreon/centreon-plugins.git
fi

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
commit=`git log -1 "$GIT_COMMIT" --pretty=format:%h`
now=`date +%s`
if [ -z "$VERSION_EXTRA" ] ; then
  export RELEASE="$now.$commit"
else
  export RELEASE="$VERSION_EXTRA.$now.$commit"
fi

# Create source tarball.
rm -rf "../centreon-$VERSION"
mkdir "../centreon-$VERSION"
git archive "$GIT_BRANCH" | tar -C "../centreon-$VERSION" -x
cd ../centreon-plugins
# We should use "$GIT_BRANCH" instead of 2.7.x. However nothing seems to work as expected.
git archive --prefix=plugins/ "origin/2.7.x" | tar -C "../centreon-$VERSION" -x
cd ..
tar czf "input/centreon-$VERSION.tar.gz" "centreon-$VERSION"

# Retrieve spec file.
if [ \! -d packaging-centreon-web ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon packaging-centreon-web
else
  cd packaging-centreon-web
  git pull
  cd ..
fi
cp packaging-centreon-web/rpm/centreon.spectemplate input/

# Retrieve additional sources.
cp packaging-centreon-web/src/* input

# Build RPMs.
docker-rpm-builder dir ci.int.centreon.com:5000/mon-build-dependencies:centos$CENTOS_VERSION input output

# Copy files to server.
if [ "$CENTOS_VERSION" = 6 ] ; then
  CES_VERSION='3'
else
  CES_VERSION='4'
fi
FILES='output/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.int.centreon.com:/srv/repos/standard/$CES_VERSION/unstable/noarch/RPMS"
DESTFILE=`ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" mktemp`
scp -o StrictHostKeyChecking=no `dirname $0`/../clean_repositories.sh "root@srvi-ces-repository.int.centreon.com:$DESTFILE"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" sh -c "'chmod +x $DESTFILE ; $DESTFILE ; rm $DESTFILE ; createrepo /srv/repos/standard/$CES_VERSION/unstable/noarch/'"
