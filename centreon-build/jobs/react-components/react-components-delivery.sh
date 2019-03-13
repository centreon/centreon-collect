#!/bin/bash

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-react-components

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "react-components/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

cd "$PROJECT-$VERSION"

cat > .npmrc << EOF
//registry.npmjs.org/:_authToken=2a5c102e-2d32-449a-9a5f-03da082f123b
EOF

npm ci
if [ "$BRANCH_NAME" == "master" ] ; then
  # if job is run from master branch, publish the next release as an alpha
  npm version $VERSION-alpha.$BUILD_NUMBER
  npm publish --access=public --tag=next ./
else
  # if job is run from another branch, publish the branch as version 0.0.0 and with tag unstable
  npm version 0.0.0-$BRANCH_NAME.$BUILD_NUMBER
  if [ "$BUILD_NUMBER" -gt "1" ] ; then
    npm deprecate @centreon/react-components@"0.0.0-$BRANCH_NAME.0 - 0.0.0-$BRANCH_NAME.$(($BUILD_NUMBER-1))" "deprecate previous versions of branch $BRANCH_NAME"
  fi
  npm publish --access=public --tag=unstable ./
fi
