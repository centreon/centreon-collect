# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "centreon-$VERSION.tar.gz" input/
cp -r packaging-centreon-web/rpm/centreon-19.4.spectemplate input/
cp -r packaging-centreon-web/src/19.4/* input/

# Build RPMs.
docker-rpm-builder dir --verbose --sign-with `dirname $0`/../../ces.key ci.int.centreon.com:5000/mon-build-dependencies-19.4:centos7 input output

# Publish RPMs.
put_internal_rpms "19.4" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
if [ "$BRANCH_NAME" '=' 'master' ] ; then
  copy_internal_rpms_to_canary "standard" "19.4" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
fi
