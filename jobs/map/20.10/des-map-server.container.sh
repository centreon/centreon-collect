#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-map-server

# Check arguments.
if [ -z "$VERSION" ] ; then
  echo "You need to specify VERSION environment variable."
  exit 1
fi

# Get into sources.
cd /usr/local/src
rm -rf "$PROJECT-$VERSION"
tar xzf "$PROJECT-$VERSION.tar.gz"
cd "$PROJECT-$VERSION"

# Run build.
mvn -f map-server-parent/pom.xml clean install
