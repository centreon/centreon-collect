#!/bin/sh

set -e
set -x

#
# This is run after the Maven build.
#

# Get version.
export VERSION=`sed -n 's/.*<version>\([0-9.]*\)<\/version>/\1/p' centreon-AS400/Connector/connector.as400/pom.xml | head -1`
SRCDIR="centreon-AS400"
SERVERDIR="centreon-connector-as400-server-$VERSION"
PLUGINDIR="ces-plugins-Operatingsystems-As400-$VERSION"

# RPM build directory.
rm -rf input-server
mkdir input-server
rm -rf input-plugin
mkdir input-plugin
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
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies:centos7 input-server output-centos7

# Build plugin.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies:centos7 input-plugin output-centos7

# Copy files to server.
FILES_CENTOS7_NOARCH='output-centos7/noarch/*.rpm'
FILES_CENTOS7_ARCH='output-centos7/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS7_NOARCH "ubuntu@srvi-repo.int.centreon.com:/srv/yum/plugin-packs/3.4/el7/testing/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7_ARCH "ubuntu@srvi-repo.int.centreon.com:/srv/yum/plugin-packs/3.4/el7/testing/x86_64/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/plugin-packs/3.4/el7/testing/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/plugin-packs/3.4/el7/testing/x86_64

# Generate testing documentation.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC 'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-as400 -V latest -p'
$SSH_DOC 'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-as400 -V latest -p'
