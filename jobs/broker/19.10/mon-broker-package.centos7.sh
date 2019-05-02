# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "$PROJECT-$VERSION.tar.gz" input/
cp `dirname $0`/../../../packaging/broker/rpm/centreon-broker-19.10.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-19.10:centos7 input output

# Publish RPMs.
put_internal_rpms "19.10" "el7" "x86_64" "broker" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "19.10" "el7" "x86_64" "broker" "$PROJECT-$VERSION-$RELEASE"
fi
