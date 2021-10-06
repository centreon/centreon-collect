#!/bin/bash

show_help() {
cat << EOF
Usage: ${0##*/} -n=[yes|no] -v

This program build Centreon-connector

    -v  : major version
    -r  : release number
    -h  : help
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

ln -s /usr/bin/cmake3 /usr/bin/cmake

# dossier racine du nouveau centreon collect
if [ ! -d /root/rpmbuild/SOURCES ] ; then
    mkdir -p /root/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
fi

cd "../../../"

mkdir centreon-connector-$VERSION
cp -r centreon-connector/* centreon-connector-$VERSION
tar -czf centreon-connector-$VERSION.tar.gz centreon-connector-$VERSION cmake.sh
mv centreon-connector-$VERSION.tar.gz /root/rpmbuild/SOURCES/
rm -rf centreon-connector-$VERSION

rpmbuild -ba centreon-connector/packaging/rpm/centreon-connector.spectemplate -D "VERSION $VERSION" -D "RELEASE $RELEASE"

