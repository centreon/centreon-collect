# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "centreon-$VERSION.tar.gz" input/
cp -r `dirname $0`/../../../packaging/web/rpm/20.04/centreon.spectemplate input/
cp -r `dirname $0`/../../../packaging/web/src/20.04/* input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-20.04:opensuse-leap input output

# Publish RPMs.
put_internal_rpms "20.04" "leap" "noarch" "web" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
