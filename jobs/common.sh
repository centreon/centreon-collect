#!/bin/sh

set -e
set -x

# Machine credentials.
REPO_CREDS="ubuntu@srvi-repo.int.centreon.com"

# Internal sources.

get_internal_source () {
  wget "http://srvi-repo.int.centreon.com/sources/internal/$1"
}

put_internal_source () {
  DIR="/srv/sources/internal/$1"
  ssh "$REPO_CREDS" mkdir -p "$DIR"
  scp "$2" "$REPO_CREDS:$DIR"
}
