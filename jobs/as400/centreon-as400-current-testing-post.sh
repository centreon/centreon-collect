#!/bin/sh

set -e
set -x

#
# This is run after the Maven build.
#

# Get version.
VERSION=`sed -n 's/.*<version>\([0-9.]*\)<\/version>/\1/p' centreon-AS400/Connector/connector.as400/pom.xml | head -1`
SRCDIR="centreon-AS400"
SERVERDIR="centreon-connector-as400-server-$VERSION"
PLUGINDIR="ces-plugins-Operatingsystems-As400"

# RPM build directory.
rm -rf input-server
mkdir input-server
rm -rf input-plugin
mkdir input-plugin
rm -rf output-centos6
mkdir output-centos6
rm -rf output-centos7
mkdir output-centos7

# Prepare server input directory.
rm -rf "$SERVERDIR"
cp -r "$SRCDIR/Connector/connector.as400.install" "$SERVERDIR"
mkdir "$SERVERDIR/bin"
cp $SRCDIR/Connector/connector.as400/target/connector-*-jar-with-dependencies.jar "$SERVERDIR/bin"
tar czf "input-server/$SERVERDIR.tar.gz" "$SERVERDIR"
cp $SRCDIR/Connector/rpm/*.spectemplate input-server/

# Prepare plugin tarball.
rm -rf "$PLUGINDIR"
cp -r "$SRCDIR/Plugins/connector.plugins" "$PLUGINDIR"
tar czf "input-plugin/$PLUGINDIR.tar.gz" "$PLUGINDIR"
cp $SRCDIR/Plugins/rpm/*.spectemplate input-plugin/

# Build server.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input-server output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input-server output-centos7

# Build plugin.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input-plugin output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input-plugin output-centos7

# Print resulting files.
find output-centos6
find output-centos7
