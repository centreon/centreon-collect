#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"
PROJECT=centreon-bi-report
# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get version.
cd centreon-bi-report
VERSION=`cat packaging/$PROJECT.spectemplate | grep Version: | cut -d ' ' -f 9`
export VERSION="$VERSION"

# Get release.
commit=`git log -1 "$GIT_COMMIT" --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$commit"

# Generate archive of Centreon MBI report.
git archive --prefix="$PROJECT-$VERSION/" "$GIT_BRANCH" | gzip > "../input/$PROJECT-$VERSION.tar.gz"
cd ..

# Pull latest build dependencies.
BUILD_IMG="ci.int.centreon.com:5000/mon-build-dependencies-3.4:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
cp centreon-bi-report/packaging/centreon-bi-report.spectemplate input/
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG" input output

# Copy files to server.
if [ "$DISTRIB" = "centos6" ] ; then
  DISTRIB='el6'
else
  DISTRIB='el7'
fi
put_internal_rpms "3.4" "$DISTRIB" "noarch" "mbi-report" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
