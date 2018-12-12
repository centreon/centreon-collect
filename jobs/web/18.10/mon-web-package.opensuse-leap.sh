# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "centreon-$VERSION.tar.gz" input/
cp -r packaging-centreon-web/rpm/centreon-18.10.spectemplate input/
cp -r packaging-centreon-web/src/18.10/* input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-18.10:opensuse-leap input output

# Publish RPMs.
put_internal_rpms "18.10" "leap" "noarch" "web" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
