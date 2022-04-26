#!/bin/bash

set -e


echo "REPO Creds ? : $REPO_CREDS"

# Machine credentials.
REPO_CREDS="ubuntu@srvi-repo.int.centreon.com"

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

  # Getting latest archive
  curl https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-4.7.0.2747-linux.zip --output sonar-scanner-cli-4.7.0.2747-linux.zip
  unzip sonar-scanner-cli-4.7.0.2747-linux.zip
  sudo rm -rf sonar-scanner-cli-4.7.0.2747-linux.zip sonar-scanner
  sudo mv sonar-scanner-4.7.0.2747-linux sonar-scanner
}

get_cache() {
  cd tmp
  echo "INFO: delete before pulling tarball ..."
  rm -rf "$PROJECT-SQ-cache-$VERSION.tar.gz"
  get_internal_source "SQ-cache/$PROJECT/$PROJECT-SQ-cache-$VERSION.tar.gz"
}

set_cache() {
  cd tmp
  if [[ -f "$PROJECT-SQ-cache-$VERSION.tar.gz" ]]; then
    echo "INFO: Saving cache's tarball ..."
    put_internal_source "SQ-cache" "$PROJECT" "$PROJECT-SQ-cache-$VERSION.tar.gz"
  else
    echo "WARNING: Tarball to save not found. Skipping ..."
  fi
}

get_internal_source() {
  URL="http://srvi-repo.int.centreon.com/sources/internal/$1"
  if validate_file_exists "$URL"; then
    wget -q "$URL"
  else
    echo "WARNING: File not found. Skipping it"
  fi
}

put_internal_source() {
  DIR="/srv/sources/internal/$1"
  NEWDIR="$2"
  ssh "$REPO_CREDS" mkdir -p "$DIR/$NEWDIR"
  shift
  shift
  scp -r "$@" "$REPO_CREDS:$DIR/$NEWDIR"
}

validate_file_exists() {
  wget --spider "$1"
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
