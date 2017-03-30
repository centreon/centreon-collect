#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$COMMIT" ] ; then
  echo "You need to specify COMMIT environment variable."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos6

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get version.
cd centreon-broker
git checkout --detach "$COMMIT"
cmakelists=build/CMakeLists.txt
major=`grep 'set(CENTREON_BROKER_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CENTREON_BROKER_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CENTREON_BROKER_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

# Create source tarball.
git archive --prefix="centreon-broker-$VERSION/" HEAD | gzip > "../input/centreon-broker-$VERSION.tar.gz"
cd ..

# Retrieve spec file.
if [ \! -d packaging-centreon-broker ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon-broker.git
else
  cd packaging-centreon-broker
  git checkout master
  git pull
  cd ..
fi
cd packaging-centreon-broker
git checkout --detach "origin/$VERSION"
cd ..
cp packaging-centreon-broker/rpm/centreon-broker.spec input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output

# Copy files to server.
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "/srv/sources/standard/testing/centreon-broker-$VERSION"
scp -o StrictHostKeyChecking=no "input/centreon-broker-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/standard/testing/centreon-broker-$VERSION/"
FILES='output/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.3/el6/testing/x86_64/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.3/el6/testing/x86_64
