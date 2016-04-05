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

# Get Centreon Broker sources.
if [ \! -d centreon-broker ] ; then
  git clone https://github.com/centreon/centreon-broker
fi

# Get version.
cmakelists=centreon-broker/build/CMakeLists.txt
major=`grep 'set(CENTREON_BROKER_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CENTREON_BROKER_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CENTREON_BROKER_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

# Get release.
cd centreon-broker
commit=`git log -1 "$GIT_COMMIT" --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$commit"

# Create source tarball.
git archive --prefix="centreon-broker-$VERSION/" "$GIT_BRANCH" | gzip > "../input/centreon-broker-$VERSION.tar.gz"
cd ..

# Retrieve spec file.
if [ \! -d packaging-centreon-broker ] ; then
  git clone http://gitbot:gitbot@git.merethis.net/packaging-centreon-broker.git
else
  cd packaging-centreon-broker
  git pull
  cd ..
fi
cp packaging-centreon-broker/rpm/centreon-broker.spectemplate input/

# Build RPMs.
docker-rpm-builder dir ci.int.centreon.com:5000/mon-build-dependencies:centos$CENTOS_VERSION input output

# Copy files to server.
if [ "$CENTOS_VERSION" = 6 ] ; then
  CES_VERSION='3'
else
  CES_VERSION='4'
fi
FILES='output/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.merethis.net:/srv/repos/standard/$CES_VERSION/testing/x86_64/RPMS"
DESTFILE=`ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net" mktemp`
scp -o StrictHostKeyChecking=no `dirname $0`/../clean_repositories.sh "root@srvi-ces-repository.merethis.net:$DESTFILE"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net" sh -c "'chmod +x $DESTFILE ; $DESTFILE ; rm $DESTFILE ; createrepo /srv/repos/standard/$CES_VERSION/testing/x86_64/'"
