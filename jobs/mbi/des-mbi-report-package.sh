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
cd centreon-bi-report
VERSION=`cat RPM-SPECS/centreon-bi-report.spec | grep Version: | cut -d ' ' -f 9`
export VERSION="$VERSION"

# Get release.
commit=`git log -1 "$GIT_COMMIT" --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$commit"

# Generate archive of Centreon MBI report.
git archive --prefix="centreon-bi-report-$VERSION/" "$GIT_BRANCH" | gzip > "../input/centreon-bi-report-$VERSION.tar.gz"
cd ..

# Pull latest build dependencies.
BUILD_IMG="ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
sed 's/@/@@/g' < centreon-bi-report/RPM-SPECS/centreon-bi-report.spec > input/centreon-bi-report.spectemplate
sed -i 's/^Release:.*$/Release: '"$RELEASE"'%{?dist}/g' input/centreon-bi-report.spectemplate
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG" input output

# Copy files to server.
if [ "$DISTRIB" = 'centos6' ] ; then
  REPO='internal/el6/noarch'
elif [ "$DISTRIB" = 'centos7' ] ; then
  REPO='internal/el7/noarch'
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
FILES='output/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "ubuntu@srvi-repo.int.centreon.com:/srv/yum/$REPO/RPMS"
DESTFILE=`ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mktemp`
scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "ubuntu@srvi-repo.int.centreon.com:$DESTFILE"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" sh $DESTFILE $REPO
