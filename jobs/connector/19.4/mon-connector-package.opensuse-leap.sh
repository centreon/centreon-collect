# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve source tarball, spectemplate and additional source files.
mv "$PROJECT-$VERSION.tar.gz" input/
cp `dirname $0`/../../../packaging/connector/centreon-connector-19.4.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key ci.int.centreon.com:5000/mon-build-dependencies-19.4:opensuse-leap input output

# Publish RPMs.
put_internal_rpms "19.4" "leap" "x86_64" "connector" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
