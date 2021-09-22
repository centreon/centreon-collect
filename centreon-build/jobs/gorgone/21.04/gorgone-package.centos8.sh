# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
rm -rf "$PROJECT-$VERSION"
tar xzf "$PROJECT-$VERSION.tar.gz"
mv "$PROJECT-$VERSION.tar.gz" input/
mv "$PROJECT-$VERSION/packaging/$PROJECT.spectemplate" input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-21.04:centos8 input output

# Create RPMs tarball.
tar czf rpms-centos8.tar.gz output
