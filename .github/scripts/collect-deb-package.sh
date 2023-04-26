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

AUTHOR="Centreon"
AUTHOR_EMAIL="contact@centreon.com"

# fix version to debian format accept
VERSION="$(echo $VERSION | sed 's/-/./g')"

MAJOR_LEFT_PART=$( echo $VERSION | cut -d "." -f1 )
MAJOR_RIGHT_PART=$( echo $VERSION | cut -d "-" -f1 | cut -d "." -f2 )
BUMP_MAJOR_RIGHT_PART=$(( MAJOR_RIGHT_PART+1 ))
if [ ${#BUMP_MAJOR_RIGHT_PART} -eq 1 ]; then
    # Add a zero before the new single numeric char
    BUMP_MAJOR_RIGHT_PART="0$BUMP_MAJOR_RIGHT_PART"
fi
MAJOR_THRESHOLD="$MAJOR_LEFT_PART.$BUMP_MAJOR_RIGHT_PART"

if [ -d "$ROOT/build" ] ; then
    rm -rf "$ROOT/build"
fi
tar --exclude={".git","build"} -czpf $ROOT-$VERSION.tar.gz "$ROOT"
cd "$ROOT"
cp -r packaging/debian debian

sed -i "s/^centreon:version=.*$/centreon:version=$(echo $VERSION-$RELEASE)/" debian/substvars
sed -i "s/^centreon:versionThreshold=.*$/centreon:versionThreshold=$(echo $MAJOR_THRESHOLD | egrep -o '^[0-9][0-9].[0-9][0-9]')/" debian/substvars

debmake -f "${AUTHOR}" -e "${AUTHOR_EMAIL}" -u "$VERSION" -r "$RELEASE"

debuild-pbuilder
