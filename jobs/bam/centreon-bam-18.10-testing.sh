#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-bam-server
curl -o centreon-translations.php 'https://raw.githubusercontent.com/centreon/centreon/master/bin/centreon-translations.php'

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ]; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies-18.10:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Get version.
cd centreon-bam
git checkout --detach "$COMMIT"
export VERSION=`grep mod_release www/modules/centreon-bam-server/conf.php | cut -d '"' -f 4`

# Create source tarball.
rm -rf "../$PROJECT-$VERSION"
mkdir "../$PROJECT-$VERSION"
git archive HEAD | tar -C "../$PROJECT-$VERSION" -x
for i in "../$PROJECT-$VERSION/www/modules/centreon-bam-server/locale"/*.UTF-8 ; do
  lang=`basename $i | cut -d _ -f 1`
  msgfmt "$i/LC_MESSAGES/messages.po" -o "$i/LC_MESSAGES/messages.mo"
  php ../centreon-translations.php $lang "$i/LC_MESSAGES/messages.po" "$i/LC_MESSAGES/messages.ser"
  rm -f "$i/LC_MESSAGES/messages.po"
done
cd ..
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

# Encrypt source tarballs.
curl -F file=@$PROJECT-$VERSION.tar.gz -F 'version=71' -F "modulename=$PROJECT" 'http://encode.int.centreon.com/api/' -o "input/$PROJECT-$VERSION-php71.tar.gz"

# Copy spec file.
cp centreon-bam/packaging/*.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-18.10:centos7 input output-centos7

# Copy files to server.
put_testing_source "bam" "bam" "$PROJECT-$VERSION-$RELEASE" "input/$PROJECT-$VERSION-php71.tar.gz"
put_testing_rpms "bam" "18.10" "el7" "noarch" "bam" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
