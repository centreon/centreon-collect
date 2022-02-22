#!/bin/sh

set -e

cmakelists=CMakeLists.txt

major=`grep 'set(COLLECT_MAJOR' CMakeLists.txt | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(COLLECT_MINOR' CMakeLists.txt | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(COLLECT_PATCH' CMakeLists.txt | cut -d ' ' -f 2 | cut -d ')' -f 1`

export VERSION="$major.$minor.$patch"

if [ -z "$VERSION" ] ; then
  echo "You need to specify the VERSION variable"
  exit 1
fi
ls -lart
sonar-scanner -Dsonar.projectVersion="$VERSION" -Dsonar.login="$1" -Dsonar.host.url="$2"