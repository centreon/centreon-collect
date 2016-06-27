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
cd centreon-ba,
VERSION=`grep mod_release www/modules/centreon-bam-server/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"

# Get release.
commit=`git log -1 "$GIT_COMMIT" --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$commit"

# Generate archive of Centreon BAM.
git archive --prefix="centreon-bam-server-$VERSION/" "$GIT_BRANCH" | gzip > "../centreon-bam-server-$VERSION.tar.gz"
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
curl -F "file=@centreon-bam-server-$VERSION.tar.gz" -F "version=$phpversion" -F 'modulename=centreon-bam-server' -F 'needlicense=0' 'http://encode.int.centreon.com/api/' -o "input/centreon-bam-server-$VERSION-php$phpversion.tar.gz"

# Pull latest build dependencies.
BUILD_IMG="ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB"
docker pull "$BUILD_IMG"

# Retrieve spec file.
if [ \! -d packaging-centreon-bam ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon-bam
else
  cd packaging-centreon-bam
  git pull
  cd ..
fi
cp packaging-centreon-bam/rpm/bam.spectemplate input

# Build RPMs.
docker-rpm-builder dir "$BUILD_IMG" input output

# Copy files to server.
if [ "$DISTRIB" = 'centos6' ] ; then
  CES_VERSION='3'
elif [ "$DISTRIB" = 'centos7' ] ; then
  CES_VERSION='4'
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
FILES='output/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.int.centreon.com:/srv/repos/standard/$CES_VERSION/unstable/noarch/RPMS"
DESTFILE=`ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" mktemp`
scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "root@srvi-ces-repository.int.centreon.com:$DESTFILE"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" sh $DESTFILE $CES_VERSION noarch
