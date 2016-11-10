#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull mon-build-dependencies container.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get Centreon Web sources.
if [ \! -d centreon-export ] ; then
  git clone http://git.int.centreon.com/centreon-export
fi

# Generate spec file.
./centreon-export/packaging/spec.sh centreon-packs.zip > input/centreon-packs.spec

# Copy source zip.
cp centreon-packs.zip input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB input output

# Remove 'fake' package.
rm -f output/noarch/centreon-pack-1.0.0*.rpm

# Copy files to server.
if [ "$DISTRIB" = 'centos6' ] ; then
  REPO='plugin-packs/dev/el6/unstable/noarch'
else
  REPO='plugin-packs/dev/el7/unstable/noarch'
fi
FILES='output/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.int.centreon.com:/srv/repos/$REPO/RPMS"
DESTFILE=`ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" mktemp`
scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "root@srvi-ces-repository.int.centreon.com:$DESTFILE"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" sh $DESTFILE $REPO
