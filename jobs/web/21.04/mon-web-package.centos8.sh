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
cp -r `dirname $0`/../../../packaging/web/rpm/21.04/centreon.spectemplate input/
cp -r `dirname $0`/../../../packaging/web/src/21.04/* input/
mv input/centreon-macroreplacement.centos8.txt input/centreon-macroreplacement.txt

# Build RPMs.
docker-rpm-builder dir --verbose --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-21.04:centos8 input output
export VERSION="$OLDVERSION"
export RELEASE="$OLDRELEASE"

# Publish RPMs.
put_internal_rpms "21.04" "el8" "noarch" "web" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "21.04" "el8" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
fi

# Create RPM tarball.
tar czf rpms-centos8.tar.gz output
