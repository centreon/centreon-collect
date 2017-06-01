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

# Internal RPMs.
put_internal_rpms () {
  DIR="/srv/yum/internal/$1/$2/$3"
  REPO="internal/$1/$2"
  shift
  shift
  shift
  ssh "$REPO_CREDS" mkdir -p "$DIR"
  scp "$@" "$REPO_CREDS:$DIR"
  DESTFILE=`ssh "$REPO_CREDS" mktemp`
  scp `dirname $0`/updaterepo.sh "$REPO_CREDS:$DESTFILE"
  ssh "$REPO_CREDS" sh $DESTFILE $REPO
}
