# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve source tarball, spectemplate and additional source files.
mv "$PROJECT-$VERSION.tar.gz" input/
cp -r `dirname $0`/../../../packaging/engine/* input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output

# Publish RPMs.
put_internal_rpms "3.4" "el7" "x86_64" "engine" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
put_internal_rpms "3.5" "el7" "x86_64" "engine" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
