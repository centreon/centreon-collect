#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$WIDGET" -o -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify WIDGET, COMMIT and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies-20.04:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Create source tarballs.
export PROJECT="centreon-widget-$WIDGET"
rm -rf "$PROJECT"
git clone "https://github.com/centreon/$PROJECT"
cd "$PROJECT"
git checkout --detach "$COMMIT"
export VERSION="`sed -n 's|\s*<version>\(.*\)</version>|\1|p' $WIDGET/configs.xml 2>/dev/null`"
export SUMMARY="`sed -n 's|\s*<description>\(.*\)</description>|\1|p' $WIDGET/configs.xml 2>/dev/null`"
rm -rf "../$PROJECT-$VERSION"
mkdir "../$PROJECT-$VERSION"
git archive HEAD | tar -C "../$PROJECT-$VERSION" -x
cd ..
tar czf "input/$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
OLDVERSION="$VERSION"
OLDRELEASE="$RELEASE"
PRERELEASE=`echo $VERSION | cut -d - -s -f 2-`
if [ -n "$PRERELEASE" ] ; then
  export VERSION=`echo $VERSION | cut -d - -f 1`
  export RELEASE="$PRERELEASE.$RELEASE"
  rm -rf "$PROJECT-$VERSION"
  mv "$PROJECT-$OLDVERSION" "$PROJECT-$VERSION"
  tar czf "input/$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
fi

# Retrieve spec file.
cp `dirname $0`/../../../packaging/widget/20.04/centreon-widget.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-20.04:centos7 input output-centos7
export VERSION="$OLDVERSION"
export RELEASE="$OLDRELEASE"

# Copy files to server.
put_testing_source "standard" "widget" "$PROJECT-$VERSION-$RELEASE" "input/$PROJECT-$VERSION.tar.gz"
put_testing_rpms "standard" "20.04" "el7" "noarch" "widget" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
