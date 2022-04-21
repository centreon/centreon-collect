#!/bin/bash
set -e

install_scanner() {
  # Installing missing requirements
  sudo apt-get install unzip || exit

  # Cleaning
  rm -rf tmp
  mkdir tmp
  cd tmp

  curl https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-4.7.0.2747-linux.zip --output sonar-scanner-cli-4.7.0.2747-linux.zip

  unzip sonar-scanner-cli-4.7.0.2747-linux.zip
  rm -rf sonar-scanner-cli-4.7.0.2747-linux.zip sonar-scanner
  sudo mv sonar-scanner-4.7.0.2747-linux sonar-scanner
}

clean_pulled_cache() {
  if [[ -d "build/cache" ]]; then
    rm -rf build/cache
  fi
}

get_cache() {
  #check if exists
  #DL file
  #decompress

  # Moving cache
  #sudo rm -rf /root/.sonar
  #mkdir -p /root/.sonar
  #mv /src/build/cache /root/.sonar/

  # Check arguments.
  if [ -z "$PROJECT" -o -z "$VERSION" ] ; then
    echo "You need to specify PROJECT and VERSION environment variables."
    exit 1
  fi

  # Fetch sources.
  cd build
  rm -rf "$PROJECT-SQ-cache-$VERSION.tar.gz"
  get_internal_source "centreon-collect/$PROJECT-$VERSION/$PROJECT-SQ-cache-$VERSION.tar.gz"
}

set_cache() {
  echo ""
  cp /root/.sonar/cache /src/build/cache
  cd /src/build/cache
  tar czf "$PROJECT-$VERSION-SQ-source.tar.gz" *

  echo "Updating SQ cache"
  put_internal_source "centreon-collect" "$PROJECT-SQ-cache-$VERSION" "$PROJECT-SQ-cache-$VERSION.tar.gz"
}

if [[ "clean" ==  "$1" ]]; then
  # maybe useless as seems mandatory
  clean_pulled_cache
elif [[ "get" == "$1" ]]; then
  get_cache
elif [[ "set" == "$1" ]]; then
  set_cache
elif [[ "install" == "$1" ]]; then
  install_scanner
fi
