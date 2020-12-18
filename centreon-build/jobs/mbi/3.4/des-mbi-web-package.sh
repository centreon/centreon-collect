#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7>"
  exit 1
fi
DISTRIB="$1"
PROJECT=centreon-bi-server

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get version.
cd centreon-bi-server
VERSION=`grep mod_release www/modules/centreon-bi-server/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"

# Get release.
commit=`git log -1 "$GIT_COMMIT" --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$commit"

# Generate archive of Centreon MBI.
git archive --prefix="centreon-bi-server-$VERSION/" "$GIT_BRANCH" | gzip > "../centreon-bi-server-$VERSION.tar.gz"
cd ..

if [ "$DISTRIB" = "centos7" ] ; then
  phpversion=54
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
curl -F "file=@centreon-bi-server-$VERSION.tar.gz" -F "version=$phpversion" -F 'modulename=centreon-bi-server-2' 'http://encode.int.centreon.com/api/' -o "input/centreon-bi-server-$VERSION-php$phpversion.tar.gz"

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-3.4:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
cp centreon-bi-server/packaging/centreon-bi-server.spectemplate input
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output

# Copy files to server.
if [ "$DISTRIB" = 'centos7' ] ; then
  DISTRIB='el7'
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi

put_internal_rpms "3.4" "$DISTRIB" "noarch" "mbi-web" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
