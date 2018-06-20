# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "centreon-$VERSION.tar.gz" input/
cp -r packaging-centreon-web/rpm/centreon-18.9.spectemplate input/
cp -r packaging-centreon-web/src/18.9/* input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key ci.int.centreon.com:5000/mon-build-dependencies-18.9:centos7 input output

# Publish RPMs.
put_internal_rpms "18.9" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
