# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "centreon-$VERSION.tar.gz" input/
cp -r packaging-centreon-web/rpm/centreon-19.04.spectemplate input/
cp -r packaging-centreon-web/src/19.04/* input/

# Build RPMs.
docker-rpm-builder dir --verbose --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-19.04:centos7 input output

# Publish RPMs.
put_internal_rpms "19.04" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "19.04" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
fi
