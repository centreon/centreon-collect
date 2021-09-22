#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$COMMIT" -o -z "$REPO" -o -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT, REPO, VERSION and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Create source tarball.
cd centreon-license-manager
git checkout --detach "$COMMIT"
rm -rf "../centreon-license-manager-$VERSION"
mkdir "../centreon-license-manager-$VERSION"
git archive HEAD | tar -C "../centreon-license-manager-$VERSION" -x
cd ..
tar czf "centreon-license-manager-$VERSION.tar.gz" "centreon-license-manager-$VERSION"

# Encrypt source archive.
curl -F "file=@centreon-license-manager-$VERSION.tar.gz" -F "version=53" -F 'modulename=centreon-license-manager' -F 'needlicense=0' 'http://encode.int.centreon.com/api/' -o "input/centreon-license-manager-$VERSION-php53.tar.gz"
curl -F "file=@centreon-license-manager-$VERSION.tar.gz" -F "version=54" -F 'modulename=centreon-license-manager' -F 'needlicense=0' 'http://encode.int.centreon.com/api/' -o "input/centreon-license-manager-$VERSION-php54.tar.gz"

cp centreon-license-manager/packaging/centreon-license-manager.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el7/$REPO/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el7/$REPO/noarch
