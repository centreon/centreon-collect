#!/bin/sh
set -e

if [ -z "$ROOT" ] ; then
  ROOT=centreon-collect
fi

if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$DISTRIB" ] ; then
  echo "You need to specify VERSION / RELEASE variables"
  exit 1
fi

echo "############################# PACKAGING COLLECT ################################"

AUTHOR="Luiz Costa"
AUTHOR_EMAIL="me@luizgustavo.pro.br"

# fix version to debian format accept
VERSION="$(echo $VERSION | sed 's/-/./g')"

if [ -d "$ROOT/build" ] ; then
    rm -rf "$ROOT/build"
fi
rm -rf "$ROOT/gorgone"
tar --exclude={".git","build"} -czpf centreon-collect-$VERSION.tar.gz "$ROOT"
cd "$ROOT"
cp -rf ci/debian-collect debian
sed -i "s/^centreon:version=.*$/centreon:version=$(echo $VERSION-$RELEASE)/" debian/substvars
echo "debmake begin"
debmake -f "${AUTHOR}" -e "${AUTHOR_EMAIL}" -u "$VERSION" -r "$RELEASE"
echo "version de dwz"
/usr/bin/dwz -v
echo "version de gcc"
gcc --version
echo "version de ld"
ld --version
debuild-pbuilder
cd ../
