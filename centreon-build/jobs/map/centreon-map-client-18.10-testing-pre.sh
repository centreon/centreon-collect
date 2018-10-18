#!/bin/sh

set -e
set -x

#
# This is run before the Maven build.
#

# Check arguments.
if [ -z "$COMMIT" ] ; then
  echo "You need to specify COMMIT environment variable."
  exit 1
fi

# Checkout commit.
cd desktop
git checkout --detach "$COMMIT"
