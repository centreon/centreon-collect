# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
OLDVERSION="$VERSION"
OLDRELEASE="$RELEASE"
PRERELEASE=`echo $VERSION | cut -d - -s -f 2-`
if [ -n "$PRERELEASE" ] ; then
  export VERSION=`echo $VERSION | cut -d - -f 1`
  export RELEASE="$PRERELEASE.$RELEASE"
fi
mv "$PROJECT-$VERSION.tar.gz" input/
cp `dirname $0`/../../../packaging/broker/rpm/21.10/centreon-broker.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-21.10:centos8 input output
export VERSION="$OLDVERSION"
export RELEASE="$OLDRELEASE"

# Publish RPMs.
put_internal_rpms "21.10" "el8" "x86_64" "broker" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "21.10" "el8" "x86_64" "broker" "$PROJECT-$VERSION-$RELEASE"
fi
