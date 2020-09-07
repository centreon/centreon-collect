#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$WIDGET" -o -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify WIDGET, COMMIT and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies-3.4:centos6
docker pull registry.centreon.com/mon-build-dependencies-3.4:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos6
mkdir output-centos6
rm -rf output-centos7
mkdir output-centos7

# Create source tarball.
export PROJECT="centreon-widget-$WIDGET"
rm -rf "$PROJECT"
git clone "https://github.com/centreon/$PROJECT"
cd "$PROJECT"
git checkout --detach "$COMMIT"
export VERSION="`sed -n 's|\s*<version>\(.*\)</version>|\1|p' $WIDGET/configs.xml 2>/dev/null`"
export SUMMARY="`sed -n 's|\s*<description>\(.*\)</description>|\1|p' $WIDGET/configs.xml 2>/dev/null`"
rm -rf "../$PROJECT-$VERSION"
mkdir "../$PROJECT-$VERSION"
git archive HEAD | tar -C "../$PROJECT-$VERSION" -x
cd ..
tar czf "input/$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

# Retrieve spec file.
cp `dirname $0`/../../packaging/widget/3.4/centreon-widget.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-3.4:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-3.4:centos7 input output-centos7

# Copy files to server.
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "/srv/sources/lts/testing/$PROJECT-$VERSION-$RELEASE"
scp -o StrictHostKeyChecking=no "input/$PROJECT-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/lts/testing/$PROJECT-$VERSION-$RELEASE/"
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/lts/3.4/el6/testing/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/lts/3.4/el7/testing/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/lts/3.4/el6/testing/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/lts/3.4/el7/testing/noarch
