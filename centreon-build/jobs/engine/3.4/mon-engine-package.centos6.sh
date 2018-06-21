# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve source tarball, spectemplate and additional source files.
mv "$PROJECT-$VERSION.tar.gz" input/
cp `dirname $0`/../../../packaging/engine/centreon-engine-3.4.spectemplate input/
cp `dirname $0`/../../../packaging/engine/centreonengine_integrate_centreon_engine2centreon.sh input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key ci.int.centreon.com:5000/mon-build-dependencies-3.4:centos6 input output

# Publish RPMs.
put_internal_rpms "3.4" "el6" "x86_64" "engine" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
