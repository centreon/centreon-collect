#!/bin/sh

set -e
set -x

#
# This is run before the Maven build.
#

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Checkout commit.
cd centreon-AS400
git checkout --detach "$COMMIT"
