#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION, RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos6
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos6
mkdir output-centos6
rm -rf output-centos7
mkdir output-centos7

# Create source tarball.
cd centreon-license-manager
git checkout --detach "tags/$VERSION"
rm -rf "../centreon-license-manager-$VERSION"
mkdir "../centreon-license-manager-$VERSION"
git archive HEAD | tar -C "../centreon-license-manager-$VERSION" -x
cd ..
tar czf "input/centreon-license-manager-$VERSION.tar.gz" "centreon-license-manager-$VERSION"

cp centreon-license-manager/packaging/centreon-license-manager.spectemplate input/

# Retrieve additional sources.
#cp centreon-license-manager/packaging/src/* input

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "root@srvi-ces-repository.int.centreon.com:/srv/repos/standard/dev/el6/testing/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "root@srvi-ces-repository.int.centreon.com:/srv/repos/standard/dev/el7/testing/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" createrepo /srv/repos/standard/dev/el6/testing/noarch
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" createrepo /srv/repos/standard/dev/el7/testing/noarch
