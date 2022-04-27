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

install_scanner() {
  # Installing missing requirements
  sudo apt-get install unzip || exit

  # Cleaning
  sudo rm -rf tmp
  mkdir tmp
  cd tmp

  echo "INFO: Getting latest archive ..."
  curl https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-4.7.0.2747-linux.zip --output sonar-scanner-cli-4.7.0.2747-linux.zip
  echo "INFO: Inflating files ..."
  unzip -q sonar-scanner-cli-4.7.0.2747-linux.zip
  echo "INFO: Cleaning ..."
  sudo rm -rf sonar-scanner-cli-4.7.0.2747-linux.zip sonar-scanner
  sudo mv sonar-scanner-4.7.0.2747-linux sonar-scanner
}

get_cache() {
  if [[ -n "$CHANGE_TARGET" ]]; then
    TARGET="$CHANGE_TARGET"
  else
    TARGET="$BRANCH_NAME"
  fi
  echo "Target: $TARGET"
  echo "CHANGE_TARGET: $CHANGE_TARGET"
  echo "CHANGE_BRANCH: $CHANGE_BRANCH"
  echo "BRANCH_NAME: $BRANCH_NAME"

  cd tmp
  echo "INFO: Cleaning before pulling tarball ..."
  rm -rf "$PROJECT-SQ-cache-$TARGET-$VERSION.tar.gz"

  PATH="SQ-cache/$PROJECT/$PROJECT-SQ-cache-$TARGET-$VERSION.tar.gz"
  URL="http://srvi-repo.int.centreon.com/sources/internal/$PATH"

echo "DEBUG: URL = $URL"

  if validate_file_exists "$URL"; then
    get_internal_source "$PATH"
  else
    echo "WARNING: File not found. Skipping it"
  fi
}

set_cache() {
  cd tmp

#CLEAN
#ssh "$REPO_CREDS" rm -f "/srv/sources/internal/SQ-cache/centreon-collect/centreon-collect-SQ-cache-22.04.0.tar.gz"
#ssh "$REPO_CREDS" rm -f "/srv/sources/internal/SQ-cache/centreon-collect/centreon-collect-SQ-cache--22.04.0.tar.gz"


  if [[ -n "$TARGET" ]]; then
    echo "FATAL: Target is empty";
    exit;
  fi

  if [[ -f "$PROJECT-SQ-cache-$TARGET-$VERSION.tar.gz" ]]; then
    echo "INFO: Saving cache's tarball ..."
    put_internal_source "SQ-cache" "$PROJECT" "$PROJECT-SQ-cache-$TARGET-$VERSION.tar.gz"
  else
    echo "WARNING: Tarball to save not found. Skipping ..."
  fi
}

validate_file_exists() {
  /usr/bin/wget --spider "$1"
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
