#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 2 ] ; then
  echo "USAGE: $0 <widget_name> <centos7|...>"
  exit 1
fi
WIDGET="$1"
DISTRIB="$2"

# Project.
PROJECT=centreon-widget-$WIDGET

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz"
get_internal_source "widgets/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
mv "$PROJECT-$VERSION.tar.gz" input/

# Retrieve spec file.
cp `dirname $0`/../../../packaging/widget/20.04/centreon-widget.spectemplate input/

# Pull mon-build-dependencies container.
docker pull registry.centreon.com/mon-build-dependencies-20.04:$DISTRIB

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-20.04:$DISTRIB input output

# Publish RPMs.
put_internal_rpms "20.04" "el7" "noarch" "widgets" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "20.04" "el7" "noarch" "widgets" "$PROJECT-$VERSION-$RELEASE"
fi
