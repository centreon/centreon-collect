# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "centreon-$VERSION.tar.gz" input/
cp -r `dirname $0`/../../../packaging/web/rpm/19.04/centreon.spectemplate input/
cp -r `dirname $0`/../../../packaging/web/src/19.04/* input/

# Build RPMs.
docker-rpm-builder dir --verbose --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-19.04:centos7 input output

# Publish RPMs.
put_internal_rpms "19.04" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "19.04" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
fi
