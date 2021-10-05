# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
OLDVERSION="$VERSION"
OLDRELEASE="$RELEASE"
PRERELEASE=`echo $VERSION | cut -d - -s -f 2-`
if [ -n "$PRERELEASE" ] ; then
  export VERSION=`echo $VERSION | cut -d - -f 1`
  export RELEASE="$PRERELEASE.$RELEASE"
fi
mv "centreon-$VERSION.tar.gz" input/
cp -r `dirname $0`/../../../packaging/web/rpm/20.10/centreon.spectemplate input/
cp -r `dirname $0`/../../../packaging/web/src/20.10/* input/
mv input/centreon-macroreplacement.centos8.txt input/centreon-macroreplacement.txt

# Build RPMs.
docker-rpm-builder dir --verbose --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-20.10:centos8 input output
export VERSION="$OLDVERSION"
export RELEASE="$OLDRELEASE"

# Create RPM tarball.
tar czf rpms-centos8.tar.gz output
