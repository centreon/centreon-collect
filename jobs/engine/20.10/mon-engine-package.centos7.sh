# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve source tarball, spectemplate and additional source files.
OLDVERSION="$VERSION"
OLDRELEASE="$RELEASE"
PRERELEASE=`echo $VERSION | cut -d - -s -f 2-`
if [ -n "$PRERELEASE" ] ; then
  export VERSION=`echo $VERSION | cut -d - -f 1`
  export RELEASE="$PRERELEASE.$RELEASE"
fi
mv "$PROJECT-$VERSION.tar.gz" input/
cp `dirname $0`/../../../packaging/engine/20.10/centreon-engine.spectemplate input/
cp `dirname $0`/../../../packaging/engine/centreonengine_integrate_centreon_engine2centreon.sh input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-20.10:centos7 input output
export VERSION="$OLDVERSION"
export RELEASE="$OLDRELEASE"

# Publish RPMs.
put_internal_rpms "20.10" "el7" "x86_64" "engine" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "20.10" "el7" "x86_64" "engine" "$PROJECT-$VERSION-$RELEASE"
fi
