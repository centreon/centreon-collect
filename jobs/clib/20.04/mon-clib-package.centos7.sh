# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "$PROJECT-$VERSION.tar.gz" input/
cp `dirname $0`/../../../packaging/clib/centreon-clib.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-20.04:centos7 input output

# Publish RPMs.
put_internal_rpms "20.04" "el7" "x86_64" "clib" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "20.04" "el7" "x86_64" "clib" "$PROJECT-$VERSION-$RELEASE"
fi
