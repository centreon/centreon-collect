#!/bin/bash
set -e

if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$DISTRIB" ] ; then
  echo "You need to specify VERSION / RELEASE variables"
  exit 1
fi

echo "################################################## BUILDING COLLECT #################################################"
# generate rpm broker
if [ ! -d /root/rpmbuild/SOURCES ] ; then
    mkdir -p /root/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
fi

mkdir centreon-collect-$VERSION
rsync -avzh --exclude .git --exclude build centreon-collect/ centreon-collect-$VERSION
tar czf centreon-collect-$VERSION.tar.gz centreon-collect-$VERSION
mv centreon-collect-$VERSION.tar.gz /root/rpmbuild/SOURCES/
rm -rf centreon-collect-$VERSION

rpmbuild -ba centreon-collect/packaging/rpm/centreon-collect.spec -D "VERSION $VERSION" -D "RELEASE $RELEASE"

# cleaning and according permissions to slave to delivery rpms
rm -rf *.rpm
cp -r /root/rpmbuild/RPMS/x86_64/*.rpm .
chmod 777 *.rpm
