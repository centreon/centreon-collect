#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$COMMIT" -o -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT, VERSION and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies-3.4:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Get Centreon Plugins sources.
if [ \! -d centreon-plugins ] ; then
  git clone https://github.com/centreon/centreon-plugins.git
fi

# Create source tarball.
cd centreon-web
git checkout --detach "$COMMIT"
rm -rf "../centreon-$VERSION" "../centreon-web-$VERSION"
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
mv "centreon-$VERSION" "centreon-web-$VERSION"
tar czf "input/centreon-web-$VERSION.tar.gz" "centreon-web-$VERSION"

# Retrieve spec file and additional sources.
cp `dirname $0`/../../packaging/web/rpm/3.4/centreon.spectemplate input/
cp `dirname $0`/../../packaging/web/src/3.4/* input

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-3.4:centos7 input output-centos7

# Copy files to server.
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "/srv/sources/standard/testing/centreon-web-$VERSION-$RELEASE"
scp -o StrictHostKeyChecking=no "input/centreon-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/standard/testing/centreon-web-$VERSION-$RELEASE/"
scp -o StrictHostKeyChecking=no "input/centreon-web-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/standard/testing/centreon-web-$VERSION-$RELEASE/"
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/lts/3.4/el7/testing/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/lts/3.4/el7/testing/noarch
