#!/bin/sh
set -e

if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$DISTRIB" ] ; then
  echo "You need to specify VERSION / RELEASE variables"
  exit 1
fi

echo "################################################## PACKAGING COLLECT ##################################################"

AUTHOR="Luiz Costa"
AUTHOR_EMAIL="me@luizgustavo.pro.br"

# fix version to debian format accept
VERSION="$(echo $VERSION | sed 's/-/./g')"

if [ -d centreon-collect/build ] ; then
    rm -rf centreon-collect/build
fi
rm -rf centreon-collect/gorgone
tar czpf centreon-collect-$VERSION.tar.gz centreon-collect
cd centreon-collect/
cp -rf ci/debian-collect debian
sed -i "s/^centreon:version=.*$/centreon:version=$(echo $VERSION-$RELEASE)/" debian/substvars
#sed -i "s/^centreon:version=.*$/centreon:version=$(echo $VERSION | egrep -o '^[0-9][0-9].[0-9][0-9]')/" debian/substvars
debmake -f "${AUTHOR}" -e "${AUTHOR_EMAIL}" -u "$VERSION" -r "$DISTRIB"
debuild-pbuilder
cd ../
