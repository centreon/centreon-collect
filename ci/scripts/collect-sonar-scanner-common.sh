#!/bin/bash
set -e

# Check arguments.
if [[ -z "$VERSION" ]] ; then
  echo "ERROR: You need to specify VERSION environment variable."
  exit 1
fi

if [[ -z "$PROJECT" ]] ; then
  echo "WARNING: PORJECT was not set as environment variable."
  PROJECT="centreon-collect"
fi

install_scanner() {
  # Installing missing requirements
  sudo apt-get install unzip || exit

  # Cleaning
  rm -rf tmp
  mkdir tmp
  cd tmp

  # Getting latest archive
  curl https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-4.7.0.2747-linux.zip --output sonar-scanner-cli-4.7.0.2747-linux.zip
  unzip sonar-scanner-cli-4.7.0.2747-linux.zip
  rm -rf sonar-scanner-cli-4.7.0.2747-linux.zip sonar-scanner
  sudo mv sonar-scanner-4.7.0.2747-linux sonar-scanner
}

clean_previous_cache() {
  if [[ -d "/src/build/cache" ]]; then
    rm -rf /src/build/cache
  fi
  if [[ -d "/root/.sonar/cache" ]]; then
    rm -rf /root/.sonar/cache
  fi
}

# workspace scope
get_cache() {
  rm -rf "$PROJECT-SQ-cache-$VERSION.tar.gz"
  get_internal_source "$PROJECT/$PROJECT-$VERSION/$PROJECT-SQ-cache-$VERSION.tar.gz"
}

#container scope
deploy_cache() {
  tar xzf "$PROJECT-SQ-cache-$VERSION.tar.gz"
  mv /src/build/cache /root/.sonar
}

# workspace scope
set_cache() {
  put_internal_source "$PROJECT" "$PROJECT-SQ-cache-$VERSION" "$PROJECT-SQ-cache-$VERSION.tar.gz"
}

# container scope
save_cache() {
  mv /root/.sonar/cache /src/build
  cd /src/build
  tar czf "$PROJECT-$VERSION-SQ-source.tar.gz" cache
}

if [[ -n $1 ]]; then
  if [[ "get" == "$1" ]]; then
    get_cache
  elif [[ "set" == "$1" ]]; then
    set_cache
  elif [[ "install" == "$1" ]]; then
    install_scanner
  fi
fi