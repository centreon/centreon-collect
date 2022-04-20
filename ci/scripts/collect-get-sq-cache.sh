#!/bin/bash
set -e

#check if exists
#DL file
#decompress



# Check arguments.
if [ -z "$PROJECT" -o -z "$VERSION" ] ; then
  echo "You need to specify PROJECT and VERSION environment variables."
  exit 1
fi


# Fetch sources.
rm -rf "$PROJECT-$VERSION-SQ-source.tar.gz"
get_internal_source "centreon-collect/$PROJECT-$VERSION/$PROJECT-$VERSION-SQ-source.tar.gz"
