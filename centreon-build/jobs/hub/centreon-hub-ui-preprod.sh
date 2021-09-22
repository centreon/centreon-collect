#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-hub-ui

# Check arguments.
if [ -z "$COMMIT" ]; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Get version.
cd centreon-hub-ui
git checkout --detach "$COMMIT"
VERSION=`grep version package.json | cut -d '"' -f 4`
export VERSION="$VERSION"

# AMAZON DEPLOYMENT

yarn install
yarn build:preprod

cd ..
tar czf "centreon-hub-ui-$VERSION.tar.gz" "centreon-hub-ui"

# Copy files to server.
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "/srv/sources/hub/testing/$PROJECT-$VERSION"
scp -o StrictHostKeyChecking=no "$PROJECT-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/hub/testing/$PROJECT-$VERSION/"

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO "cd /tmp && rm -rf './$PROJECT' && tar zxf /srv/sources/hub/testing/$PROJECT-$VERSION/$PROJECT-$VERSION.tar.gz && aws s3 sync '/tmp/$PROJECT/build' 's3://ppd-centreon-hub-ui' --delete"
