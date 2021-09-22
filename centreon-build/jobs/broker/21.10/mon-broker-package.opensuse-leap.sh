# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "$PROJECT-$VERSION.tar.gz" input/
cp `dirname $0`/../../../packaging/broker/rpm/21.10/centreon-broker.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-21.10:opensuse-leap input output

# Publish RPMs.
put_internal_rpms "21.10" "leap" "x86_64" "broker" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
