#!/bin/bash

show_help() {
cat << EOF
Usage: ${0##*/} version release

This program build Centreon-collect
EOF
exit 2
}

VERSION=$1
RELEASE=$2

if [ -z $VERSION ] || [ -z $RELEASE ] ; then
   echo "Some or all of the parameters are empty";
   echo $VERSION;
   echo $RELEASE;
   show_help
fi

# root dir of the new centreon collect
if [ ! -d /root/rpmbuild/SOURCES ] ; then
    mkdir -p /root/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
fi

cd ../../..

mkdir centreon-collect-$VERSION
rsync -avzh --exclude .git --exclude build centreon-collect/ centreon-collect-$VERSION
tar czf centreon-collect-$VERSION.tar.gz centreon-collect-$VERSION
mv centreon-collect-$VERSION.tar.gz /root/rpmbuild/SOURCES/
rm -rf centreon-collect-$VERSION

rpmbuild -ba centreon-collect/packaging/rpm/centreon-collect.spec -D "VERSION $VERSION" -D "RELEASE $RELEASE"

