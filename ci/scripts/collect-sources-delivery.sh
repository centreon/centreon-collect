#!/bin/bash
set -e

. ./common.sh

cmakelists=$PROJECT/CMakeLists.txt

major=`grep 'set(COLLECT_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(COLLECT_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(COLLECT_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`

export VERSION="$major.$minor.$patch"
export MAJOR="$major.$minor"

COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
if [ "$BUILD" '=' 'RELEASE' ] ; then
    export RELEASE="$BUILD_NUMBER"
else
    export RELEASE="$now.$COMMIT"
fi

echo -n "#####GET $PROJECT COMMITER#####"
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

echo -n "#####ARCHIVING $PROJECT#####"
tar czf "centreon-collect-$VERSION.tar.gz" *   

echo -n "#####DELIVER $PROJECT SOURCES#####"
put_internal_source "$PROJECT" "centreon-collect-$VERSION-$RELEASE" "centreon-collect-$VERSION.tar.gz"

echo -n "#####EXPORTING $PROJECT GLOBAL VARIABLES#####"
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
MAJOR=$MAJOR
EOF



