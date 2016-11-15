#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$NAME" ] ; then
  echo "You need to specify VERSION, RELEASE, NAME environment variables."
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
rm -rf $NAME
git clone https://github.com/Centreon-Widgets/$NAME.git $NAME
cd $NAME
git checkout --detach "tags/$VERSION"
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

export WIDGETSUBDIR=`echo "$NAME" | sed 's/centreon-widget-//'`

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "root@srvi-ces-repository.int.centreon.com:/srv/repos/standard/3.4/el6/unstable/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "root@srvi-ces-repository.int.centreon.com:/srv/repos/standard/3.4/el7/unstable/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" createrepo /srv/repos/standard/3.4/el6/unstable/noarch
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" createrepo /srv/repos/standard/3.4/el7/unstable/noarch
