# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
mv "$PKGNAME-$VERSION.tar.gz" input/
cp `dirname $0`/../../packaging/vmware/$PROJECT.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-20.10:centos8 input output

# Publish RPMs.
put_internal_rpms "20.10" "el8" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
put_internal_rpms "21.04" "el8" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
put_internal_rpms "21.10" "el8" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
if [ "$BRANCH_NAME" '=' 'master' ] ; then
  copy_internal_rpms_to_unstable "standard" "20.10" "el8" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_unstable "standard" "21.04" "el8" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_unstable "standard" "21.10" "el8" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
fi
