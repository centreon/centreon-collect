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
cd centreon-export
git checkout --detach "tags/$VERSION"
rm -rf "../centreon-export-$VERSION"
mkdir "../centreon-export-$VERSION"
git archive HEAD | tar -C "../centreon-export-$VERSION" -x
cd ..
tar czf "centreon-export-$VERSION.tar.gz" "centreon-export-$VERSION"

# Encrypt source archive.
curl -F "file=@centreon-export-$VERSION.tar.gz" -F "version=53" -F 'modulename=centreon-export' -F 'needlicense=0' 'http://encode.int.centreon.com/api/' -o "input/centreon-export-$VERSION-php53.tar.gz"
curl -F "file=@centreon-export-$VERSION.tar.gz" -F "version=54" -F 'modulename=centreon-export' -F 'needlicense=0' 'http://encode.int.centreon.com/api/' -o "input/centreon-export-$VERSION-php54.tar.gz"

cp centreon-export/packaging/centreon-export.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "root@srvi-ces-repository.int.centreon.com:/srv/repos/standard/3.4/el6/stable/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "root@srvi-ces-repository.int.centreon.com:/srv/repos/standard/3.4/el7/stable/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" createrepo /srv/repos/standard/3.4/el6/stable/noarch
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" createrepo /srv/repos/standard/3.4/el7/stable/noarch
