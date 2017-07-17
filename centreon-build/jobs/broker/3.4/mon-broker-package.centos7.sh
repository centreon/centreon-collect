# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
cd input
get_internal_source "broker/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
cd ..
cp packaging-centreon-broker/rpm/centreon-broker.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output

# Publish RPMs.
put_internal_rpms "3.4" "el7" "x86_64" "broker" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
put_internal_rpms "3.5" "el7" "x86_64" "broker" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
