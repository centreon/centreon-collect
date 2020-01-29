#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-documentation

# Get release.
cd "$PROJECT"
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$COMMIT"

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`
cd ..

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
