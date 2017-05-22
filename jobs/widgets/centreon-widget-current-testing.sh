#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$WIDGET" -o -z "$COMMIT" -o -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify WIDGET, COMMIT, VERSION and RELEASE environment variables."
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
NAME="centreon-widget-$WIDGET"
rm -rf "$NAME"
git clone "https://github.com/Centreon-Widgets/$NAME"
cd "$NAME"
git checkout --detach "$COMMIT"
rm -rf "../$NAME-$VERSION"
mkdir "../$NAME-$VERSION"
git archive HEAD | tar -C "../$NAME-$VERSION" -x
cd ..
tar czf "input/$NAME-$VERSION.tar.gz" "$NAME-$VERSION"

# Retrieve spec file.
if [ \! -d packaging-centreon-web ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon packaging-centreon-web
else
  cd packaging-centreon-web
  git pull
  cd ..
fi
cp packaging-centreon-web/rpm/widgets/centreon-widget.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el6/testing/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el7/testing/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el6/testing/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el7/testing/noarch
