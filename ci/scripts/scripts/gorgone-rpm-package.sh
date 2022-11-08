#!/bin/sh
set -ex

if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION / RELEASE / DISTRIB variables"
  exit 1
fi

echo "################################################## PACKAGING GORGONE ##################################################"


# generate rpm broker
if [ ! -d /root/rpmbuild/SOURCES ] ; then
    mkdir -p /root/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
fi
cd /src/gorgone

tar czvpf /root/rpmbuild/SOURCES/centreon-gorgone-$VERSION.tar.gz \
      --transform "s,^\.,centreon-gorgone-$VERSION," .

rpmbuild -ba /src/gorgone/packaging/centreon-gorgone.spectemplate -D "VERSION $VERSION" -D "REL $RELEASE"

# cleaning and according permissions to slave to delivery rpms
cd /src
rm -rf *.rpm
cp -r /root/rpmbuild/RPMS/noarch/*.rpm .
chmod 777 *.rpm