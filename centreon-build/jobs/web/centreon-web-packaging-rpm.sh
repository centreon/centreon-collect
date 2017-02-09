#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$COMMIT" -o -z "$REPO" -o -z "$VERSION" -o -z "$RELEASE" ] ; then
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

# Get Centreon Plugins sources.
if [ \! -d centreon-plugins ] ; then
  git clone https://github.com/centreon/centreon-plugins.git
fi

# Create source tarball.
cd centreon-web
git checkout --detach "$COMMIT"
rm -rf "../centreon-$VERSION"
mkdir "../centreon-$VERSION"
git archive HEAD | tar -C "../centreon-$VERSION" -x
cd ../centreon-plugins
# We should use "$GIT_BRANCH" instead of 2.7.x. However nothing seems to work as expected.
git archive --prefix=plugins/ "origin/2.7.x" | tar -C "../centreon-$VERSION" -x
# Generate release notes.
# Code adapted from centreon-tools/make_package.sh.
cd "../centreon-$VERSION/doc/en"
make SPHINXOPTS="-D html_theme=scrolls" html
major=`echo "$VERSION" | cut -d . -f 1`
minor=`echo "$VERSION" | cut -d . -f 2`
cp "_build/html/release_notes/centreon-$major.$minor/centreon-$VERSION.html" "../../www/install/RELEASENOTES.html"
sed -i \
    -e "/<link/d" \
    -e "/<script .*>.*<\/script>/d" \
    -e "s/href=\"..\//href=\"http:\/\/documentation.centreon.com\/docs\/centreon\/en\/latest\//g" \
    -e "/<\/head>/i \
    <style type=\"text/css\">\n \
    #toc, .footer, .relnav, .header { display: none; }\n \
    <\/style>" ../../www/install/RELEASENOTES.html
make clean
cd ../../..
tar czf "input/centreon-$VERSION.tar.gz" "centreon-$VERSION"

# Retrieve spec file.
if [ \! -d packaging-centreon-web ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon packaging-centreon-web
else
  cd packaging-centreon-web
  git pull
  cd ..
fi
cd packaging-centreon-web
git checkout --detach "origin/$VERSION"
cd ..
cp packaging-centreon-web/rpm/centreon.spectemplate input/

# Retrieve additional sources.
cp packaging-centreon-web/src/* input

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "/srv/sources/standard/$REPO/centreon-web-$VERSION-$RELEASE"
scp -o StrictHostKeyChecking=no "input/centreon-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/standard/$REPO/centreon-web-$VERSION-$RELEASE/"
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el6/$REPO/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el7/$REPO/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el6/$REPO/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el7/$REPO/noarch
