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

# Sources.

get_internal_source () {
  wget "http://srvi-repo.int.centreon.com/sources/internal/$1"
}

put_internal_source () {
  DIR="/srv/sources/internal/$1"
  NEWDIR="$2"
  ssh "$REPO_CREDS" mkdir -p "$DIR/$NEWDIR"
  shift
  shift
  scp "$@" "$REPO_CREDS:$DIR/$NEWDIR"
  clean_directory "$DIR"
}

put_testing_source () {
  DIR="/srv/sources/$1/testing/$2"
  NEWDIR="$3"
  ssh "$REPO_CREDS" mkdir -p "$DIR/$NEWDIR"
  shift
  shift
  shift
  scp "$@" "$REPO_CREDS:$DIR/$NEWDIR"
}

# Packages.

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
  ssh "$REPO_CREDS" createrepo "$DIR/$NEWDIR"
}

put_testing_rpms () {
  DIR="/srv/yum/$1/$2/$3/testing/$4/$5"
  NEWDIR="$6"
  REPO="$1/$2/$3/testing/$4"
  REPOROOT="$1"
  REPOSUBDIR="/$2/$3/testing/$4"
  shift
  shift
  shift
  shift
  shift
  shift
  ssh "$REPO_CREDS" mkdir -p "$DIR/$NEWDIR"
  scp "$@" "$REPO_CREDS:$DIR/$NEWDIR"
  DESTFILE=`ssh "$REPO_CREDS" mktemp`
  UPDATEREPODIR=`dirname $0`
  while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
    UPDATEREPODIR="$UPDATEREPODIR/.."
  done
  scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
  ssh "$REPO_CREDS" sh $DESTFILE $REPO
  ssh "$REPO_CREDS" "/srv/scripts/sync-$REPOROOT.sh" --confirm "$REPOSUBDIR"
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

copy_internal_rpms_to_canary () {
  TARGETDIR="/srv/yum/$1/$2/$3/canary/$4/$5"
  REPO="$1/$2/$3/canary/$4"
  ssh "$REPO_CREDS" cp -r "/srv/yum/internal/$2/$3/$4/$5/$6" "$TARGETDIR/"
  clean_directory "$TARGETDIR"
  DESTFILE=`ssh "$REPO_CREDS" mktemp`
  UPDATEREPODIR=`dirname $0`
  while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
    UPDATEREPODIR="$UPDATEREPODIR/.."
  done
  scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
  ssh "$REPO_CREDS" sh $DESTFILE $REPO
  ssh "$REPO_CREDS" "/srv/scripts/sync-$1.sh" --confirm "/$2/$3/canary/$4"
}

copy_internal_rpms_to_unstable () {
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
  ssh "$REPO_CREDS" "/srv/scripts/sync-$1.sh" --confirm "/$2/$3/unstable/$4"
}

promote_canary_rpms_to_unstable () {
  TARGETDIR="/srv/yum/$1/$2/$3/unstable/$4/$5"
  REPO="$1/$2/$3/unstable/$4"
  ssh "$REPO_CREDS" cp -r "/srv/yum/$1/$2/$3/canary/$4/$5/$6" "$TARGETDIR/"
  clean_directory "$TARGETDIR"
  DESTFILE=`ssh "$REPO_CREDS" mktemp`
  UPDATEREPODIR=`dirname $0`
  while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
    UPDATEREPODIR="$UPDATEREPODIR/.."
  done
  scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
  ssh "$REPO_CREDS" sh $DESTFILE $REPO
  ssh "$REPO_CREDS" "/srv/scripts/sync-$1.sh" --confirm "/$2/$3/unstable/$4"
}

promote_unstable_rpms_to_testing () {
  TARGETDIR="/srv/yum/$1/$2/$3/testing/$4/$5"
  REPO="$1/$2/$3/testing/$4"
  ssh "$REPO_CREDS" cp -r "/srv/yum/$1/$2/$3/unstable/$4/$5/$6" "$TARGETDIR/"
  clean_directory "$TARGETDIR"
  DESTFILE=`ssh "$REPO_CREDS" mktemp`
  UPDATEREPODIR=`dirname $0`
  while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
    UPDATEREPODIR="$UPDATEREPODIR/.."
  done
  scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
  ssh "$REPO_CREDS" sh $DESTFILE $REPO
  ssh "$REPO_CREDS" "/srv/scripts/sync-$1.sh" --confirm "/$2/$3/testing/$4"
}

promote_testing_rpms_to_stable () {
  SOURCEDIR="/srv/yum/$1/$2/$3/testing/$4/$5/$6"
  TARGETDIR="/srv/yum/$1/$2/$3/stable/$4/RPMS"
  REPO="$1/$2/$3/stable/$4"
  ssh "$REPO_CREDS" cp "$SOURCEDIR/*.rpm" "$TARGETDIR/"
  DESTFILE=`ssh "$REPO_CREDS" mktemp`
  UPDATEREPODIR=`dirname $0`
  while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
    UPDATEREPODIR="$UPDATEREPODIR/.."
  done
  scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
  ssh "$REPO_CREDS" sh $DESTFILE $REPO
  ssh "$REPO_CREDS" "/srv/scripts/sync-$1.sh" --confirm "/$2/$3/stable/$4"
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
