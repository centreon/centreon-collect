#!/bin/sh

set -e
set -x

# Prepare source directory.
cd /usr/local/src
tar xzf "$PROJECT-$VERSION.tar.gz"

# build frontend
cd "$PROJECT-$VERSION"
npm run build

# Create source tarballs.
cd ..
rm -f "$PROJECT-$VERSION.tar.gz"
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
