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
   echo "$VERSION";
   echo "$RELEASE";
   show_help
fi

# root dir of the new centreon collect
if [ ! -d /root/rpmbuild/SOURCES ] ; then
    mkdir -p /root/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
fi

cd ../..

tar czf "/root/rpmbuild/SOURCES/centreon-collect-$VERSION.tar.gz" \
      --exclude './build' \
      --exclude './.git'  \
      --transform "s,^\.,centreon-collect-$VERSION," .

cp packaging/rpm/centreonengine_integrate_centreon_engine2centreon.sh /root/rpmbuild/SOURCES/

echo -e "%_topdir      %(echo $HOME)/rpmbuild\n%_smp_mflags  -j5\n" > $HOME/rpmbuild/.rpmmacros

cd ..

rpmbuild -ba centreon-collect/packaging/rpm/centreon-collect.spec -D "VERSION $VERSION" -D "RELEASE $RELEASE"

