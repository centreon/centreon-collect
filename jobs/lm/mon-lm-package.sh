#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get version.
cd centreon-license-manager
VERSION=`grep mod_release www/modules/centreon-license-manager/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"

# Get release.
commit=`git log -1 "$GIT_COMMIT" --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$commit"

# Generate archive of Centreon LM.
git archive --prefix="centreon-license-manager-$VERSION/" "$GIT_BRANCH" | gzip > "../centreon-license-manager-$VERSION.tar.gz"
cd ..

# Encrypt source archive.
if [ "$DISTRIB" = "centos6" ] ; then
  phpversion=53
elif [ "$DISTRIB" = "centos7" ] ; then
  phpversion=54
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
curl -F "file=@centreon-license-manager-$VERSION.tar.gz" -F "version=$phpversion" -F 'modulename=centreon-license-manager' -F 'needlicense=0' 'http://encode.int.centreon.com/api/' -o "input/centreon-license-manager-$VERSION-php$phpversion.tar.gz"

# Pull latest build dependencies.
BUILD_IMG="ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
cp centreon-license-manager/packaging/centreon-license-manager.spectemplate input
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG" input output

# Copy files to server.
if [ "$DISTRIB" = 'centos6' ] ; then
  REPO='standard/dev/el6/unstable/noarch'
elif [ "$DISTRIB" = 'centos7' ] ; then
  REPO='standard/dev/el7/unstable/noarch'
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
FILES='output/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.int.centreon.com:/srv/repos/$REPO/RPMS"
DESTFILE=`ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" mktemp`
scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "root@srvi-ces-repository.int.centreon.com:$DESTFILE"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" sh $DESTFILE $REPO
