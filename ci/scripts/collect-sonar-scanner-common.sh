#!/bin/bash

set -e

source common.sh

# Check arguments.
if [[ -z "$VERSION" ]] ; then
  echo "ERROR: You need to specify VERSION environment variable."
  exit 1
fi

if [[ -z "$PROJECT" ]] ; then
  echo "WARNING: PROJECT was not set as environment variable."
  PROJECT="centreon-collect"
fi

if [[ -n "$CHANGE_TARGET" ]]; then
  TARGET="$CHANGE_TARGET"
else
  TARGET="$BRANCH_NAME"
fi

install_scanner() {
  echo "INFO: Cleaning tmp ..."
  sudo rm -rf "/$WORKSPACE/centreon-collect-centos7/tmp"
  mkdir tmp
  cd tmp

  echo "INFO: Installing missing requirements ..."
  sudo apt-get install unzip || exit

  echo "INFO: Getting latest archive ..."
  curl https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-4.7.0.2747-linux.zip --output sonar-scanner-cli-4.7.0.2747-linux.zip
  echo "INFO: Inflating files ..."
  unzip -q sonar-scanner-cli-4.7.0.2747-linux.zip
  echo "INFO: Cleaning again ..."
  sudo rm -rf sonar-scanner-cli-4.7.0.2747-linux.zip sonar-scanner
  sudo mv sonar-scanner-4.7.0.2747-linux sonar-scanner
}

get_cache() {
  
  echo $(pwd)
  echo "INFO: Cleaning before pulling tarball ..."
  rm -f "$PROJECT-SQ-cache-$TARGET.tar.gz"

  CACHE_PATH="SQ-cache/$PROJECT/$PROJECT-SQ-cache-$TARGET.tar.gz"
  CACHE_URL="http://srvi-repo.int.centreon.com/sources/internal/$CACHE_PATH"

  if validate_file_exists "$CACHE_URL"; then
    echo "INFO: Pulling tarball ..."
    wget -q "$CACHE_URL"
  else
    echo "WARNING: File not found. Skipping $TARGET's cache on $VERSION"
  fi
}

set_cache() {
  
  echo $(pwd)
  if [[ -z "$TARGET" ]]; then
    echo "ERROR: Target's name is empty. Skipping $VERSION's cache"
    exit
  fi

  if [[ -f "$PROJECT-SQ-cache-$TARGET.tar.gz" ]]; then
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
if [[ -n "$1" ]]; then
  if [[ "get" == "$1" ]]; then
    get_cache
  elif [[ "set" == "$1" ]]; then
    set_cache
  elif [[ "install" == "$1" ]]; then
    install_scanner
  fi
fi
