#!/bin/sh

set -e
set -x

# Machine credentials.
REPO_CREDS="ubuntu@srvi-repo.int.centreon.com"

# Cleanup routine.

clean_directory () {
  rm -rf `ls -rc "$1" | head -n -10`
}

# Internal sources.

get_internal_source () {
  wget "http://srvi-repo.int.centreon.com/sources/internal/$1"
}

put_internal_source () {
  DIR="/srv/sources/internal/$1"
  NEWDIR="$2"
  ssh "$REPO_CREDS" mkdir -p "$DIR/$NEWDIR"
  scp "$3" "$REPO_CREDS:$DIR/$NEWDIR"
  clean_directory "$DIR"
}

# Internal packages.

put_internal_rpms () {
  DIR="/srv/yum/internal/$1/$2/$3/$4"
  NEWDIR="$5"
  REPO="internal/$1/$2/$3"
  shift
  shift
  shift
  shift
  shift
  ssh "$REPO_CREDS" mkdir -p "$DIR/$NEWDIR"
  scp "$@" "$REPO_CREDS:$DIR/$NEWDIR"
  clean_directory "$DIR"
  DESTFILE=`ssh "$REPO_CREDS" mktemp`
  UPDATEREPODIR=`dirname $0`
  while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
    UPDATEREPODIR="$UPDATEREPODIR/.."
  done
  scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
  ssh "$REPO_CREDS" sh $DESTFILE $REPO
}

put_internal_debs () {
  DIR="/srv/apt/internal/$1"
  DISTRIB="$2"
  shift
  shift
  for deb in $@ ; do
    scp "$deb" "$REPO_CREDS:/tmp/"
    ssh "$REPO_CREDS" "cd $DIR && reprepro includedeb $DISTRIB /tmp/`basename $deb`"
  done
}
