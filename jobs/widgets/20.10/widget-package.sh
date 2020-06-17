#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$WIDGET" ] ; then
  echo "You need to specify VERSION, RELEASE and WIDGET environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|centos8|...>"
  exit 1
fi
DISTRIB="$1"
case "$DISTRIB" in
  centos7)
    DISTRIBCODENAME=el7
    ;;
  centos8)
    DISTRIBCODENAME=el8
    ;;
  *)
    echo "Unsupported distribution $DISTRIB"
    exit 1
esac

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
cp `dirname $0`/../../../packaging/widget/20.10/centreon-widget.spectemplate input/

# Pull mon-build-dependencies container.
docker pull registry.centreon.com/mon-build-dependencies-20.10:$DISTRIB

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-20.10:$DISTRIB input output

# Publish RPMs.
put_internal_rpms "20.10" "$DISTRIBCODENAME" "noarch" "widgets" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "20.10" "$DISTRIBCODENAME" "noarch" "widgets" "$PROJECT-$VERSION-$RELEASE"
fi

# Create RPMs tarball.
tar czf "rpms-$DISTRIB.tar.gz" output
