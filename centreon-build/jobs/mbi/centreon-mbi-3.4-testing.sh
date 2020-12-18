#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" -o -z "$PROJECT" ]; then
  echo "You need to specify COMMIT, RELEASE and PROJECT environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies-3.4:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Create source tarball.
rm -rf $PROJECT
git clone https://centreon-bot:518bc6ce608956da1eadbe71ff7de731474b773b@github.com/centreon/$PROJECT
cd $PROJECT
git checkout --detach "$COMMIT"

# Fetch version.
if [ "$PROJECT" = "centreon-bi-server" ]; then
  export VERSION=`grep mod_release www/modules/centreon-bi-server/conf.php | cut -d '"' -f 4`
else
  export VERSION=`cat packaging/*.spectemplate | grep Version: | rev | cut -f 1 | cut -d ' ' -f 1 | rev`
fi

rm -rf "../$PROJECT-$VERSION"
mkdir "../$PROJECT-$VERSION"
git archive HEAD | tar -C "../$PROJECT-$VERSION" -x
cd ..
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

if [ "$PROJECT" = "centreon-bi-server" ]; then
  curl -F file=@centreon-bi-server-$VERSION.tar.gz -F 'version=53' -F 'modulename=centreon-bi-server-2' 'http://encode.int.centreon.com/api/' -o centreon-bi-server-$VERSION-php53.tar.gz
  curl -F file=@centreon-bi-server-$VERSION.tar.gz -F 'version=54' -F 'modulename=centreon-bi-server-2' 'http://encode.int.centreon.com/api/' -o centreon-bi-server-$VERSION-php54.tar.gz
  ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "/srv/sources/mbi/testing/$PROJECT-$VERSION-$RELEASE"
  scp -o StrictHostKeyChecking=no $PROJECT-$VERSION*.tar.gz "ubuntu@srvi-repo.int.centreon.com:/srv/sources/mbi/testing/$PROJECT-$VERSION-$RELEASE"
fi

if [ "$generateRPM" = "No" ]; then
    echo "[RPM Will not be copied to any external repositories]"
    exit 0
fi

cp $PROJECT-$VERSION*.tar.gz input/
cp $PROJECT/packaging/*.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-3.4:centos7 input output-centos7

# Copy files to server.
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/mbi/3.4/el7/testing/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/mbi/3.4/el7/testing/noarch

# Generate documentation on doc-dev.
if [ "$DOCUMENTATION" '!=' 'false' ] ; then
  SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-bi-2 -V latest -p'"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-bi-2 -V latest -p'"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-bi-2 -V 3.2.x -p'"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-bi-2 -V 3.2.x -p'"
fi
