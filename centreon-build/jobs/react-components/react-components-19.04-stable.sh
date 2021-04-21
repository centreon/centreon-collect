#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-react-components

# Check arguments.
if [ -z "$COMMIT" ]; then
  echo "You need to specify COMMIT environment variables."
  exit 1
fi

# Get version.
cd $PROJECT
git checkout --detach "$COMMIT"
export VERSION=$(cat package.json | grep version | head -1 | awk -F: '{ print $2 }' | sed 's/[",]//g' | tr -d '[[:space:]]')

cat > .npmrc << EOF
//registry.npmjs.org/:_authToken=2a5c102e-2d32-449a-9a5f-03da082f123b
EOF

# install dependencies
npm ci

# release on npm registry
npm publish --access=public --tag=latest ./

# push tag on github if $COMMIT is not a tag
if ! git tag --list | egrep -q "^$COMMIT$" ; then
  git config remote.origin.url ssh://git@github.com/centreon/centreon-react-components
  git tag $VERSION
  git push origin $VERSION
fi

