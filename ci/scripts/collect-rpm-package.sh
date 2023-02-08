#!/bin/bash
set -e

if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$DISTRIB" ] ; then
  echo "You need to specify VERSION / RELEASE / DISTRIB variables"
  exit 1
fi

echo "########################### BUILDING COLLECT ############################"

#yum install wget unzip
#wget https://github.com/ninja-build/ninja/releases/download/v1.11.1/ninja-linux.zip
#unzip ninja-linux.zip
#mv ninja /usr/bin
#chmod 755 /usr/bin/ninja


# generate rpm broker
if [ ! -d ~/rpmbuild/SOURCES ] ; then
    mkdir -p ~/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
fi

tar czf ~/rpmbuild/SOURCES/centreon-collect-$VERSION.tar.gz \
      --exclude './build' \
      --exclude './.git'  \
      --transform "s,^\.,centreon-collect-$VERSION," .

cp packaging/rpm/centreonengine_integrate_centreon_engine2centreon.sh ~/rpmbuild/SOURCES/

rpmbuild -ba packaging/rpm/centreon-collect.spec -D "VERSION $VERSION" -D "RELEASE $RELEASE"

cp -r ~/rpmbuild/RPMS/x86_64/*.rpm .
