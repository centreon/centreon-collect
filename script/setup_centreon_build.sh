#!/bin/sh

# Update to latest build scripts.
cd /opt/centreon-build
git pull
cd -

# Copy centreon-build directory.
rm -rf centreon-build
cp -r /opt/centreon-build .

# Try to checkout current project branch in centreon-build.
cd centreon-build
git checkout "$BRANCH_NAME" || true
cd ..

# There you go !
