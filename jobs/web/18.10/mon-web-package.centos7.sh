# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "centreon-$VERSION.tar.gz" input/
cp -r `dirname $0`/../../../packaging/web/rpm/18.10/centreon.spectemplate input/
cp -r `dirname $0`/../../../packaging/web/src/18.10/* input/

# Build RPMs.
docker-rpm-builder dir --verbose --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-18.10:centos7 input output

# Publish RPMs.
put_internal_rpms "18.10" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "18.10" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
fi
