#!/bin/sh

set -e
set -x

# Machine credentials.
REGISTRY="registry.centreon.com"
REPO_CREDS="ubuntu@srvi-repo.int.centreon.com"
DLDEV_URL='http://download-dev.int.centreon.com'
DL_URL='https://download.centreon.com'

# Variables.
export COMPOSE_HTTP_TIMEOUT=180

# Cleanup routine.

clean_directory () {
  CMD='ls -drc '"$1/*"' | head -n -6 | xargs rm -rf'
  ssh "$REPO_CREDS" "$CMD"
}

# Sources.

get_internal_source () {
  rm -f `basename $1`
  wget -q "http://srvi-repo.int.centreon.com/sources/internal/$1"
}

put_internal_source () {
  DIR="/srv/sources/internal/$1"
  NEWDIR="$2"
  ssh "$REPO_CREDS" mkdir -p "$DIR/$NEWDIR"
  shift
  shift
  scp -r "$@" "$REPO_CREDS:$DIR/$NEWDIR"
  clean_directory "$DIR"
}

put_testing_source () {
  DIR="/srv/sources/$1/testing/$2"
  NEWDIR="$3"
  ssh "$REPO_CREDS" mkdir -p "$DIR/$NEWDIR"
  shift
  shift
  shift
  scp -r "$@" "$REPO_CREDS:$DIR/$NEWDIR"
}

copy_internal_source_to_testing () {
  SRCDIR="/srv/sources/internal/$2/$3"
  DESTDIR="/srv/sources/$1/testing/$2/"
  ssh "$REPO_CREDS" cp -r "$SRCDIR" "$DESTDIR"
}

# Expect arguments in the following order:
#   1. project as in centreon-download (centreon-web)
#   2. serie (20.04)
#   3. version (20.04.3)
#   4. extension (tar.gz)
#   5. ddos protection enabled (0 or 1)
#   6. source file on srvi-repo
#   7. destination file on S3
upload_artifact_for_download () {
  SRCHASH=`ssh $REPO_CREDS "md5sum $6 | cut -d ' ' -f 1"`
  SRCSIZE=`ssh $REPO_CREDS "stat -c '%s' $6"`
  ssh $REPO_CREDS aws s3 cp --acl public-read "$6" "$7"
  curl "https://download.centreon.com/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$1&release=$2&version=$3&extension=$4&md5=$SRCHASH&size=$SRCSIZE&ddos=$5&dryrun=0"
}

# More concise version of the above, specialy made for source tarballs.
#   1. project
#   2. version
#   3. source file
#   4. destination file
upload_tarball_for_download () {
  SERIE=`echo $2 | cut -d . -f 1-2`
  upload_artifact_for_download "$1" "$SERIE" "$2" tar.gz 0 "$3" "$4"
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
    ssh "$REPO_CREDS" "mkdir -p $DIR && cd $DIR && reprepro --waitforlock 10 includedeb $DISTRIB /tmp/`basename $deb` && rm -f /tmp/`basename $deb`"
  done
}

copy_internal_rpms_to_canary () {
  TARGETDIR="/srv/yum/$1/$2/$3/canary/$4/$5"
  REPO="$1/$2/$3/canary/$4"
  ssh "$REPO_CREDS" mkdir -p "$TARGETDIR"
  ssh "$REPO_CREDS" cp -r "/srv/yum/internal/$2/$3/$4/$5/$6" "$TARGETDIR/"
  ssh "$REPO_CREDS" rm -rf "$TARGETDIR/$6/repodata"
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
  ssh "$REPO_CREDS" mkdir -p "$TARGETDIR"
  ssh "$REPO_CREDS" cp -r "/srv/yum/internal/$2/$3/$4/$5/$6" "$TARGETDIR/"
  ssh "$REPO_CREDS" rm -rf "$TARGETDIR/$6/repodata"
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

copy_internal_rpms_to_testing () {
  TARGETDIR="/srv/yum/$1/$2/$3/testing/$4/$5"
  REPO="$1/$2/$3/testing/$4"
  ssh "$REPO_CREDS" mkdir -p "$TARGETDIR"
  ssh "$REPO_CREDS" cp -r "/srv/yum/internal/$2/$3/$4/$5/$6" "$TARGETDIR/"
  ssh "$REPO_CREDS" rm -rf "$TARGETDIR/$6/repodata"
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

promote_canary_rpms_to_unstable () {
  TARGETDIR="/srv/yum/$1/$2/$3/unstable/$4/$5"
  REPO="$1/$2/$3/unstable/$4"
  ssh "$REPO_CREDS" mkdir -p "$TARGETDIR"
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
  ssh "$REPO_CREDS" mkdir -p "$TARGETDIR"
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
  ssh "$REPO_CREDS" mkdir -p "$TARGETDIR"
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

# Summary report.

generate_summary() {
  sed -i -e "s#@REGISTRY@#registry.centreon.com#g" -e "s#@REPOSITORY@#http://srvi-repo.int.centreon.com#g" -e "s#@PROJECT@#$PROJECT#g" -e "s#@VERSION@#$VERSION#g" -e "s#@RELEASE@#$RELEASE#g" summary/jobData.json
  php summary/index.php > summary/index.html
  rm -f summary/index.php summary/jobData.json
}
