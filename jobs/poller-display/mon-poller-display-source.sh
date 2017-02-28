#!/bin/sh

set -e
set -x

#
# This script will generate Centreon Poller Display sources from the
# local clone of Centreon Poller Display repository
# (centreon-poller-display directory). These sources will then be pushed
# to the internal repository (srvi-repo) and used in downstream jobs,
# thanks to the property file generated at the end of the script.
#

# Get version.
cd centreon-poller-display
VERSION=`grep mod_release www/modules/centreon-poller-display/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"

# Get release.
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$COMMIT"

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Create source tarball.
git archive --prefix="centreon-poller-display-$VERSION/" HEAD | gzip > "../centreon-poller-display-$VERSION.tar.gz"
cd ..

# Send it to srvi-repo.
FILES="centreon-poller-display-$VERSION.tar.gz"
DEST="/srv/sources/internal/centreon-poller-display-$VERSION-$RELEASE"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "$DEST"
scp -o StrictHostKeyChecking=no $FILES "ubuntu@srvi-repo.int.centreon.com:$DEST"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=centreon-poller-display
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
