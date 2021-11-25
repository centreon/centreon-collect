#!/bin/bash
set -e

. ./common.sh


echo -n "#####The Delivered project is centreon-collect#####"
echo -n "#####GET centreon-collect VERSION#####"

cmakelists=CMakeLists.txt

major=`grep 'set(COLLECT_MAJOR' CMakeLists.txt | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(COLLECT_MINOR' CMakeLists.txt | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(COLLECT_PATCH' CMakeLists.txt | cut -d ' ' -f 2 | cut -d ')' -f 1`

export VERSION="$major.$minor.$patch"
export MAJOR="$major.$minor"


echo -n "#####GET centreon-collect RELEASE#####"

COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
if [ "$BUILD" '=' 'RELEASE' ] ; then
    export RELEASE="$BUILD_NUMBER"
else
    export RELEASE="$now.$COMMIT"
fi

echo -n "#####GET centreon-collect COMMITER#####"
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`


echo -n "#####ARCHIVING centreon-collect#####"
rm -rf centreon-collect-$VERSION-$RELEASE.tar.gz
tar czf "centreon-collect-$VERSION-$RELEASE.tar.gz" *    

echo -n "#####DELIVER centreon-collect SOURCES#####"
put_internal_source "centreon-collect" "centreon-collect-$VERSION-$RELEASE" "centreon-collect-$VERSION.tar.gz"

echo -n "#####EXPORTING centreon-collect GLOBAL VARIABLES#####"
cat > source.properties << EOF
PROJECT=centreon-collect
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
MAJOR=$MAJOR
EOF