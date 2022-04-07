#!/bin/sh
set -e

if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$DISTRIB" ] ; then
  echo "You need to specify VERSION / RELEASE variables"
  exit 1
fi

echo "################################################## PACKAGING COLLECT ##################################################"

AUTHOR="Luiz Costa"
AUTHOR_EMAIL="me@luizgustavo.pro.br"

if [ -d centreon-collect/build ] ; then
    rm -rf centreon-collect/build
fi
tar czpf centreon-collect-$VERSION.tar.gz centreon-collect
cd centreon-collect/
cp -rf ci/debian .
debmake -f "${AUTHOR}" -e "${AUTHOR_EMAIL}" -u "$VERSION" -r "$RELEASE"
debuild-pbuilder
cd ../
if [ -d "$DISTRIB" ] ; then
  rm -rf "$DISTRIB"
fi
mkdir $DISTRIB
mv *.deb $DISTRIB/
