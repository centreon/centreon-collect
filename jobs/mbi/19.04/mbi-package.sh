#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-mbi

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|centos8|...>"
  exit 1
fi
DISTRIB="$1"

# Create input and output directories.
for i in engine etl report reporting-server server ; do
  rm -rf input-$i
  mkdir input-$i
  rm -rf output-$i
  mkdir output-$i
done

# Fetch sources.
for i in engine etl report reporting-server ; do
  get_internal_source "mbi/$PROJECT-$VERSION-$RELEASE/centreon-bi-$i-$VERSION.tar.gz"
  rm -rf "centreon-bi-$i-$VERSION"
  tar xzf "centreon-bi-$i-$VERSION.tar.gz"
  mv "centreon-bi-$i-$VERSION.tar.gz" "input-$i/"
  mv "centreon-bi-$i-$VERSION/packaging/centreon-bi-$i.spectemplate" "input-$i/"
done
cd input-server
get_internal_source "mbi/$PROJECT-$VERSION-$RELEASE/centreon-bi-server-$VERSION-php71.tar.gz"
get_internal_source "mbi/$PROJECT-$VERSION-$RELEASE/centreon-bi-server.spectemplate"
cd ..

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-19.04:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
echo 'engine
etl
report
reporting-server
server' | parallel docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input-{} output-{}

# Copy files to server.
if [ "$DISTRIB" = 'centos7' ] ; then
  DISTRIB=el7
elif [ "$DISTRIB" = 'centos8' ] ; then
  DISTRIB=el8
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi

put_internal_rpms "19.04" "$DISTRIB" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE" output-engine/noarch/*.rpm output-etl/noarch/*.rpm output-report/noarch/*.rpm output-reporting-server/noarch/*.rpm output-server/noarch/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "mbi" "19.04" "$DISTRIB" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE"
fi
