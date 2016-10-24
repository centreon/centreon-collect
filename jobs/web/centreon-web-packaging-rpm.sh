#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$BRANCH" ] ; then
  echo "You need to specify VERSION, RELEASE and BRANCH environment variables."
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

# Get Centreon Web sources.
if [ \! -d centreon-web ] ; then
  git clone https://github.com/centreon/centreon centreon-web
fi

# Get Centreon Plugins sources.
if [ \! -d centreon-plugins ] ; then
  git clone https://github.com/centreon/centreon-plugins.git
fi

# Create source tarball.
cd centreon-web
rm -rf "../centreon-$VERSION"
mkdir "../centreon-$VERSION"
git archive "$BRANCH" | tar -C "../centreon-$VERSION" -x
cd ../centreon-plugins
# We should use "$GIT_BRANCH" instead of 2.7.x. However nothing seems to work as expected.
git archive --prefix=plugins/ "origin/2.7.x" | tar -C "../centreon-$VERSION" -x
cd ..
tar czf "input/centreon-$VERSION.tar.gz" "centreon-$VERSION"

# Retrieve spec file.
if [ \! -d packaging-centreon-web ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon packaging-centreon-web
else
  cd packaging-centreon-web
  git pull
  cd ..
fi
cp packaging-centreon-web/rpm/centreon.spectemplate input/

# Retrieve additional sources.
cp packaging-centreon-web/src/* input

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
