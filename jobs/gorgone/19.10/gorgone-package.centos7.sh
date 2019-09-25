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
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-19.10:centos7 input output

# Publish RPMs.
put_internal_rpms "19.10" "el7" "noarch" "gorgone" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
if [ "$BRANCH_NAME" '=' 'master' ] ; then
  copy_internal_rpms_to_unstable "standard" "19.10" "el7" "noarch" "gorgone" "$PROJECT-$VERSION-$RELEASE"
fi
