#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-AS400

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Get version.
export VERSION=`sed -n 's/.*<version>\([0-9.]*\)<\/version>/\1/p' centreon-AS400/Connector/connector.as400/pom.xml | head -1`
SRCDIR="centreon-AS400"
SERVERDIR="centreon-connector-as400-server-$VERSION"
PLUGINDIR="ces-plugins-Operatingsystems-As400-$VERSION"

# Pull mon-build-dependencies containers.
BUILD_CENTOS7=registry.centreon.com/mon-build-dependencies-21.04:centos7
docker pull "$BUILD_CENTOS7"
BUILD_CENTOS8=registry.centreon.com/mon-build-dependencies-21.04:centos8
docker pull "$BUILD_CENTOS8"

# RPM build directory.
rm -rf input-server
mkdir input-server
rm -rf input-plugin
mkdir input-plugin
rm -rf output-centos7
mkdir output-centos7
rm -rf output-centos8
mkdir output-centos8

# Prepare server input directory.
rm -rf "$SERVERDIR"
cp -r "$SRCDIR/Connector/connector.as400.install" "$SERVERDIR"
mkdir "$SERVERDIR/bin"
cp $SRCDIR/Connector/connector.as400/target/connector-*-jar-with-dependencies.jar "$SERVERDIR/bin"
tar czf "input-server/$SERVERDIR.tar.gz" "$SERVERDIR"
cp $SRCDIR/Connector/rpm/*.spectemplate input-server/

# Build server.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS7" input-server output-centos7
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS8" input-server output-centos8

# Build plugin.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS7" input-plugin output-centos7
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS8" input-plugin output-centos8

# 20.04
put_testing_rpms "standard" "20.04" "el7" "x86_64" "centreon-as400" "centreon-as400" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "20.04" "el7" "noarch" "centreon-as400" "centreon-as400" output-centos7/noarch/*.rpm
put_testing_rpms "standard" "20.04" "el8" "x86_64" "centreon-as400" "centreon-as400" output-centos8/x86_64/*.rpm
put_testing_rpms "standard" "20.04" "el8" "noarch" "centreon-as400" "centreon-as400" output-centos8/noarch/*.rpm

# 20.10
put_testing_rpms "standard" "20.10" "el7" "x86_64" "centreon-as400" "centreon-as400" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "20.10" "el7" "noarch" "centreon-as400" "centreon-as400" output-centos7/noarch/*.rpm
put_testing_rpms "standard" "20.10" "el8" "x86_64" "centreon-as400" "centreon-as400" output-centos8/x86_64/*.rpm
put_testing_rpms "standard" "20.10" "el8" "noarch" "centreon-as400" "centreon-as400" output-centos8/noarch/*.rpm

# 21.04
put_testing_rpms "standard" "21.04" "el7" "x86_64" "centreon-as400" "centreon-as400" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "21.04" "el7" "noarch" "centreon-as400" "centreon-as400" output-centos7/noarch/*.rpm
put_testing_rpms "standard" "21.04" "el8" "x86_64" "centreon-as400" "centreon-as400" output-centos8/x86_64/*.rpm
put_testing_rpms "standard" "21.04" "el8" "noarch" "centreon-as400" "centreon-as400" output-centos8/noarch/*.rpm