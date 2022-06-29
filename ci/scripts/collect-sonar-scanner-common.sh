#!/bin/bash

set -e

source common.sh
CACHE_REFERENCE="$2"
# Check arguments.
if [ -z "$VERSION" ]; then
  echo "ERROR: You need to specify VERSION environment variable."
  exit 1
fi

if [ -z "$PROJECT" ]; then
  echo "WARNING: PROJECT was not set as environment variable."
  PROJECT="centreon-collect"
fi

if [ -n "$CHANGE_TARGET" ]; then
  TARGET="$CHANGE_TARGET"
else
  TARGET="$BRANCH_NAME"
fi

get_cache() {
  echo "INFO: Cleaning before pulling tarball ..."
  rm -f "$PROJECT-SQ-cache-$TARGET.tar.gz"

  TAR_NAME="$PROJECT-SQ-cache-$TARGET.tar.gz"
  CACHE_URL="http://srvi-repo.int.centreon.com/sources/internal/SQ-cache/$PROJECT/$TAR_NAME"
  if validate_file_exists "$CACHE_URL"; then
    echo "INFO: Pulling tarball ..."
    wget -q "$CACHE_URL"
  else
    TAR_NAME="$PROJECT-SQ-cache-$CACHE_REFERENCE.tar.gz"
    CACHE_URL="http://srvi-repo.int.centreon.com/sources/internal/SQ-cache/$PROJECT/$TAR_NAME"
    if validate_file_exists "$CACHE_URL"; then
      echo "INFO: Pulling tarball $CACHE_REFERENCE ..."
      wget -q "$CACHE_URL"
    else
      echo "WARNING: File not found. Skipping $TARGET's cache on $CACHE_REFERENCE"
      if [ ! -d build ] ; then
        mkdir build
      fi
      return 0
    fi 
  fi
  tar xzf "$TAR_NAME"
  mkdir build
  mv cache build/
  rm -rf "$TAR_NAME"
}

set_cache() {
  cd tmp
  if [ -z "$TARGET" ]; then
    echo "ERROR: Target's name is empty. Skipping $VERSION's cache"
    exit
  fi

  if [ -f "$PROJECT-SQ-cache-$TARGET.tar.gz" ]; then
    echo "INFO: Saving cache's tarball $PROJECT-SQ-cache-$TARGET.tar.gz ..."
    put_internal_source "SQ-cache" "$PROJECT" "$PROJECT-SQ-cache-$TARGET.tar.gz"
  else
    echo "WARNING: Tarball to save not found. Skipping $TARGET's cache"
  fi
}

validate_file_exists() {
  wget --spider -q "$1"
  return $?
}

# case load
if [ -n "$1" ]; then
  if [ "get" == "$1" ]; then
    get_cache
  elif [ "set" == "$1" ]; then
    set_cache
  fi
fi
