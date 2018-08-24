#!/bin/sh

set -e
set -x

# Machine credentials.
REPO_CREDS="ubuntu@srvi-repo.int.centreon.com"

# Variables.
export COMPOSE_HTTP_TIMEOUT=180

# Cleanup routine.

clean_directory () {
  CMD='ls -rc '"$1"' | head -n -10 | xargs rm -rf'
  ssh "$REPO_CREDS" "$CMD"
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
    ssh "$REPO_CREDS" "cd $DIR && reprepro includedeb $DISTRIB /tmp/`basename $deb` && rm -f /tmp/`basename $deb`"
  done
}

move_internal_rpms_to_unstable () {
  TARGETDIR="/srv/yum/$1/$2/$3/unstable/$4/$5"
  REPO="$1/$2/$3/unstable/$4"
  ssh "$REPO_CREDS" cp -r "/srv/yum/internal/$2/$3/$4/$5/$6" "$TARGETDIR/"
  clean_directory "$TARGETDIR"
  DESTFILE=`ssh "$REPO_CREDS" mktemp`
  UPDATEREPODIR=`dirname $0`
  while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
    UPDATEREPODIR="$UPDATEREPODIR/.."
  done
  scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
  ssh "$REPO_CREDS" sh $DESTFILE $REPO
}

# Acceptance tests.

launch_webdriver() {
  set +e
  nodes=0
  while [ -z "$nodes" -o "$nodes" -lt 4 ] ; do
    docker-compose -f "$1" -p webdriver up -d --scale 'chrome=4'
    sleep 10
    nodes=`curl 'http://localhost:4444/grid/api/hub' | python -m json.tool | grep free | cut -d ':' -f 2 | tr -d ' ,'`
    if [ -z "${nodes}" ]; then
      nodes=0
    fi
  done
  set -e
}

stop_webdriver() {
  set +e
  docker-compose -f "$1" -p webdriver down
  set -e
}
