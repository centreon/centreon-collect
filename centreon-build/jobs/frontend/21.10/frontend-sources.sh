#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

#
# This script will generate sources from the local clone of the project
# repository. These sources will then be pushed to the internal
# repository (srvi-repo) and used in downstream jobs, thanks to the
# property file generated at the end of the script.
#

# Project.
PROJECT=centreon-frontend
cd $PROJECT

# Create sources tarball
rm -rf "$PROJECT-$VERSION"
mkdir "$PROJECT-$VERSION"
git archive HEAD | tar -C "$PROJECT-$VERSION" -x
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

# Send it to srvi-repo.
put_internal_source "frontend" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
