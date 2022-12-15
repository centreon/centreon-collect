#!/bin/sh
set -e

if [ -z "$ROOT" ] ; then
  ROOT=centreon-collect
fi

if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$DISTRIB" ] ; then
  echo "You need to specify VERSION / RELEASE / DISTRIB variables"
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
tar --exclude={".git","build"} -czpf $ROOT-$VERSION.tar.gz "$ROOT"
cd "$ROOT"
cp -r ci/debian debian

sed -i "s/^centreon:version=.*$/centreon:version=$(echo $VERSION-$RELEASE)/" debian/substvars
debmake -f "${AUTHOR}" -e "${AUTHOR_EMAIL}" -u "$VERSION" -r "$DISTRIB"
debuild-pbuilder
cd ../
