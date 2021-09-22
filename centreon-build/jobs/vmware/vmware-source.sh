#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

#
# This script will generate sources from the local clone of the project
# repository. These sources will then be pushed to the internal
# repository (srvi-repo) and used in downstream jobs, thanks to the
# property file generated at the end of the script.
#

# Project.
PROJECT=centreon-vmware

# Get version.
cd "$PROJECT"
export VERSION=`grep VERSION centreon/script/centreon_vmware.pm | cut -d "'" -f 2`

# Get release.
COMMIT=`git log -1 HEAD --pretty=format:%h`
export RELEASE=`date '+%Y%m%d%H%M%S'`

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Create source tarball.
PKGNAME='centreon-plugin-Virtualization-VMWare-daemon'
git archive --prefix="$PKGNAME-$VERSION/" HEAD | gzip > "../$PKGNAME-$VERSION.tar.gz"
cd ..

# Send it to srvi-repo.
put_internal_source "vmware" "$PROJECT-$VERSION-$RELEASE" "$PKGNAME-$VERSION.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
