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
cp -r `dirname $0`/../../../packaging/web/rpm/21.10/centreon.spectemplate input/
cp -r `dirname $0`/../../../packaging/web/src/21.10/* input/
mv input/centreon-macroreplacement.centos7.txt input/centreon-macroreplacement.txt

# Build RPMs.
docker-rpm-builder dir --verbose --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-21.10:centos7 input output
export VERSION="$OLDVERSION"
export RELEASE="$OLDRELEASE"

# Create RPM tarball.
tar czf rpms-centos7.tar.gz output
