#!/bin/sh

PROJECT=centreon-bam-server

set -e
set -x

# Check arguments.
if [ -z "$COMMIT" -o -z "$REPO" -o -z "$VERSION" -o -z "$RELEASE" ]; then
  echo "You need to specify COMMIT, REPO, VERSION and RELEASE environment variables."
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

# Checkout project
rm -rf centreon-bam
git clone https://centreon-bot:518bc6ce608956da1eadbe71ff7de731474b773b@github.com/centreon/centreon-bam



# Create source tarball.
cd centreon-bam
git checkout --detach "$COMMIT"
rm -rf "../$PROJECT-$VERSION"
mkdir "../$PROJECT-$VERSION"
git archive HEAD | tar -C "../$PROJECT-$VERSION" -x
cd ..
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

curl -F file=@centreon-bam-server-$VERSION.tar.gz -F 'version=53' -F 'modulename=centreon-bam-server' 'http://encode.int.centreon.com/api/' -o centreon-bam-server-$VERSION-php53.tar.gz
curl -F file=@centreon-bam-server-$VERSION.tar.gz -F 'version=54' -F 'modulename=centreon-bam-server' 'http://encode.int.centreon.com/api/' -o centreon-bam-server-$VERSION-php54.tar.gz
scp -o StrictHostKeyChecking=no $PROJECT-$VERSION*.tar.gz "ubuntu@srvi-repo.int.centreon.com:/srv/sources/bam/$REPO/"

if [ "$generateRPM" = "No" ]; then
	echo "[RPM Will not be generated and copied to any external repositories]"
    exit 0
fi

cp $PROJECT-$VERSION*.tar.gz input/
cp centreon-bam/packaging/*.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'



scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/bam/3.4/el6/$REPO/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/bam/3.4/el7/$REPO/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/bam/3.4/el6/$REPO/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/bam/3.4/el7/$REPO/noarch
