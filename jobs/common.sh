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
  if [ "$1" '=' 'bam' -o "$1" '=' 'map' -o "$1" '=' 'mbi' ] ; then
    TARGETPROJECTS="$1 business"
  else
    TARGETPROJECTS="$1"
  fi
  SERIE="$2"
  OS="$3"
  ARCH="$4"
  PRODUCT="$5"
  GROUP="$6"
  shift 6
  DESTFILE=`ssh "$REPO_CREDS" mktemp`
  UPDATEREPODIR=`dirname $0`
  while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
    UPDATEREPODIR="$UPDATEREPODIR/.."
  done
  for TARGETPROJECT in $TARGETPROJECTS ; do
    DIR="/srv/yum/$TARGETPROJECT/$SERIE/$OS/testing/$ARCH/$PRODUCT"
    NEWDIR="$GROUP"
    REPO="$TARGETPROJECT/$SERIE/$OS/testing/$ARCH"
    REPOROOT="$TARGETPROJECT"
    REPOSUBDIR="/$SERIE/$OS/testing/$ARCH"
    ssh "$REPO_CREDS" mkdir -p "$DIR/$NEWDIR"
    scp "$@" "$REPO_CREDS:$DIR/$NEWDIR"
    scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
    ssh "$REPO_CREDS" sh $DESTFILE $REPO
    $UPDATEREPODIR/sync-repo.sh --project $REPOROOT --path "$REPOSUBDIR" --confirm
  done
}

put_internal_debs () {
  DIR="/srv/apt/internal/$1"
  DISTRIB="$2"
  shift
  shift
  for deb in $@ ; do
    scp "$deb" "$REPO_CREDS:/tmp/"
    ssh "$REPO_CREDS" "mkdir -p $DIR && cd $DIR && reprepro --waitforlock 10 -Vb . includedeb $DISTRIB /tmp/`basename $deb` && rm -f /tmp/`basename $deb`"
  done
}

copy_internal_rpms_to_canary () {
  if [ "$1" '=' 'bam' -o "$1" '=' 'map' -o "$1" '=' 'mbi' ] ; then
    TARGETPROJECTS="$1 business"
  else
    TARGETPROJECTS="$1"
  fi

  for TARGETPROJECT in $TARGETPROJECTS ; do
    TARGETDIR="/srv/yum/$TARGETPROJECT/$2/$3/canary/$4/$5"
    REPO="$TARGETPROJECT/$2/$3/canary/$4"
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
    $UPDATEREPODIR/sync-repo.sh --project $TARGETPROJECT --path "/$2/$3/canary/$4" --confirm
  done
}

copy_internal_rpms_to_unstable () {
  if [ "$1" '=' 'bam' -o "$1" '=' 'map' -o "$1" '=' 'mbi' ] ; then
    TARGETPROJECTS="$1 business"
  else
    TARGETPROJECTS="$1"
  fi

  for TARGETPROJECT in $TARGETPROJECTS ; do
    TARGETDIR="/srv/yum/$TARGETPROJECT/$2/$3/unstable/$4/$5"
    REPO="$TARGETPROJECT/$2/$3/unstable/$4"
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
    $UPDATEREPODIR/sync-repo.sh --project $TARGETPROJECT --path "/$2/$3/unstable/$4" --confirm
  done
}

copy_internal_rpms_to_testing () {
  if [ "$1" '=' 'bam' -o "$1" '=' 'map' -o "$1" '=' 'mbi' ] ; then
    TARGETPROJECTS="$1 business"
  else
    TARGETPROJECTS="$1"
  fi

  for TARGETPROJECT in $TARGETPROJECTS ; do
    TARGETDIR="/srv/yum/$TARGETPROJECT/$2/$3/testing/$4/$5"
    REPO="$TARGETPROJECT/$2/$3/testing/$4"
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
    $UPDATEREPODIR/sync-repo.sh --project $TARGETPROJECT --path "/$2/$3/testing/$4" --confirm
  done
}

promote_canary_rpms_to_unstable () {
  if [ "$1" '=' 'bam' -o "$1" '=' 'map' -o "$1" '=' 'mbi' ] ; then
    TARGETPROJECTS="$1 business"
  else
    TARGETPROJECTS="$1"
  fi

  for TARGETPROJECT in $TARGETPROJECTS ; do
    TARGETDIR="/srv/yum/$TARGETPROJECT/$2/$3/unstable/$4/$5"
    REPO="$TARGETPROJECT/$2/$3/unstable/$4"
    ssh "$REPO_CREDS" mkdir -p "$TARGETDIR"
    ssh "$REPO_CREDS" cp -r "/srv/yum/$TARGETPROJECT/$2/$3/canary/$4/$5/$6" "$TARGETDIR/"
    clean_directory "$TARGETDIR"
    DESTFILE=`ssh "$REPO_CREDS" mktemp`
    UPDATEREPODIR=`dirname $0`
    while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
      UPDATEREPODIR="$UPDATEREPODIR/.."
    done
    scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
    ssh "$REPO_CREDS" sh $DESTFILE $REPO
    $UPDATEREPODIR/sync-repo.sh --project $TARGETPROJECT --path "/$2/$3/unstable/$4" --confirm
  done
}

promote_unstable_rpms_to_testing () {
  if [ "$1" '=' 'bam' -o "$1" '=' 'map' -o "$1" '=' 'mbi' ] ; then
    TARGETPROJECTS="$1 business"
  else
    TARGETPROJECTS="$1"
  fi

  for TARGETPROJECT in $TARGETPROJECTS ; do
    TARGETDIR="/srv/yum/$TARGETPROJECT/$2/$3/testing/$4/$5"
    REPO="$1/$2/$3/testing/$4"
    ssh "$REPO_CREDS" mkdir -p "$TARGETDIR"
    ssh "$REPO_CREDS" cp -r "/srv/yum/$TARGETPROJECT/$2/$3/unstable/$4/$5/$6" "$TARGETDIR/"
    clean_directory "$TARGETDIR"
    DESTFILE=`ssh "$REPO_CREDS" mktemp`
    UPDATEREPODIR=`dirname $0`
    while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
      UPDATEREPODIR="$UPDATEREPODIR/.."
    done
    scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
    ssh "$REPO_CREDS" sh $DESTFILE $REPO
    $UPDATEREPODIR/sync-repo.sh --project $TARGETPROJECT --path "/$2/$3/testing/$4" --confirm
  done
}

promote_testing_rpms_to_stable () {
  if [ "$1" '=' 'bam' -o "$1" '=' 'map' -o "$1" '=' 'mbi' ] ; then
    TARGETPROJECTS="$1 business"
  else
    TARGETPROJECTS="$1"
  fi

  for TARGETPROJECT in $TARGETPROJECTS ; do
    SOURCEDIR="/srv/yum/$TARGETPROJECT/$2/$3/testing/$4/$5/$6"
    TARGETDIR="/srv/yum/$TARGETPROJECT/$2/$3/stable/$4/RPMS"
    REPO="$TARGETPROJECT/$2/$3/stable/$4"
    ssh "$REPO_CREDS" mkdir -p "$TARGETDIR"
    ssh "$REPO_CREDS" cp "$SOURCEDIR/*.rpm" "$TARGETDIR/"
    DESTFILE=`ssh "$REPO_CREDS" mktemp`
    UPDATEREPODIR=`dirname $0`
    while [ \! -f "$UPDATEREPODIR/updaterepo.sh" ] ; do
      UPDATEREPODIR="$UPDATEREPODIR/.."
    done
    scp "$UPDATEREPODIR/updaterepo.sh" "$REPO_CREDS:$DESTFILE"
    ssh "$REPO_CREDS" sh $DESTFILE $REPO
    $UPDATEREPODIR/sync-repo.sh --project $TARGETPROJECT --path "/$2/$3/stable/$4" --confirm
  done
}

# Summary report.

generate_summary() {
  sed -i -e "s#@REGISTRY@#registry.centreon.com#g" -e "s#@REPOSITORY@#http://srvi-repo.int.centreon.com#g" -e "s#@PROJECT@#$PROJECT#g" -e "s#@VERSION@#$VERSION#g" -e "s#@RELEASE@#$RELEASE#g" summary/jobData.json
  php summary/index.php > summary/index.html
  rm -f summary/index.php summary/jobData.json
}




#Functions used by the new CI/CD workflow

put_rpms () {

  # "REPO" "$MAJOR" "DISTRIB" "REPOTYPE" "ARCH" "PROJECT" "FOLDER" 'RPMS'

  # Checking if we push standard rpms or business modules
  if [ "$1" = "standard" ] ; then
    PROJECT_PATH=standard
  elif [ "$1" = "business" ] ; then
    PROJECT_PATH=centreon-business/1a97ff9985262bf3daf7a0919f9c59a6
  elif [ "$1" = "lts" ] ; then
    PROJECT_PATH=centreon-lts/1c0143e9c46ef424bc0c1ae9ba77203c17abf1cda7e
  elif [ "$1" = "bam" ] ; then
    PROJECT_PATH=centreon-bam/d4e1d7d3e888f596674453d1f20ff6d3
  elif [ "$1" = "map" ] ; then
    PROJECT_PATH=centreon-map/bfcfef6922ae08bd2b641324188d8a5f
  elif [ "$1" = "mbi" ] ; then
    PROJECT_PATH=centreon-mbi/5e0524c1c4773a938c44139ea9d8b4d7
  elif [ "$1" = "plugin-packs" ] ; then
    PROJECT_PATH=plugin-packs/2e83f5ff110c44a9cab8f8c7ebbe3c4f
  fi

  #Local variables for the parameters that are used to create the targer path
  MAJOR=$2
  DISTRIB=$3
  REPOTYPE=$4
  ARCH=$5
  PROJECT=$6
  FOLDER=$7
  #Local variable for rpm put
  TARGET="/srv/centreon-yum/yum.centreon.com/$PROJECT_PATH/$MAJOR/$DISTRIB/$REPOTYPE/$ARCH/$PROJECT/$FOLDER"
  #Local variable for cleaning project directory to keep only six rpms projects
  PROJECT_LOCATION="/srv/centreon-yum/yum.centreon.com/$PROJECT_PATH/$MAJOR/$DISTRIB/$REPOTYPE/$ARCH/$PROJECT"
  METADATAS="/srv/centreon-yum/yum.centreon.com/$PROJECT_PATH/$MAJOR/$DISTRIB/$REPOTYPE/$ARCH"
  ssh -o StrictHostKeyChecking=no "cesync@yum.int.centreon.com" mkdir -p "$TARGET"
  shift 8
  scp -o StrictHostKeyChecking=no "$@" "cesync@yum.int.centreon.com:$TARGET"
  ssh -o StrictHostKeyChecking=no "cesync@yum.int.centreon.com" "ls -drc $PROJECT_LOCATION/* | head -n -6 | xargs rm -rf"
  ssh -o StrictHostKeyChecking=no "cesync@yum.int.centreon.com" createrepo "$METADATAS"
  ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" aws cloudfront create-invalidation --distribution-id E34EBWWERP6QET --paths "/$PROJECT_PATH/$MAJOR/$DISTRIB/$REPOTYPE/$ARCH/*"
}

promote_rpms_from_testing_to_stable () {

  # Checking if we push standard rpms or business modules
  if [ "$1" = "standard" ] ; then
    PROJECT_PATH=standard
  elif [ "$1" = "business" ] ; then
    PROJECT_PATH=centreon-business/1a97ff9985262bf3daf7a0919f9c59a6
  elif [ "$1" = "lts" ] ; then
    PROJECT_PATH=centreon-lts/1c0143e9c46ef424bc0c1ae9ba77203c17abf1cda7e
  elif [ "$1" = "bam" ] ; then
    PROJECT_PATH=centreon-bam/d4e1d7d3e888f596674453d1f20ff6d3
  elif [ "$1" = "map" ] ; then
    PROJECT_PATH=centreon-map/bfcfef6922ae08bd2b641324188d8a5f
  elif [ "$1" = "mbi" ] ; then
    PROJECT_PATH=centreon-mbi/5e0524c1c4773a938c44139ea9d8b4d7
  elif [ "$1" = "plugin-packs" ] ; then
    PROJECT_PATH=plugin-packs/2e83f5ff110c44a9cab8f8c7ebbe3c4f
  fi
  
  #Local variables for the parameters that are used to create the targer path
  MAJOR=$2
  DISTRIB=$3
  ARCH=$4
  PROJECT=$5
  FOLDER=$6

  #Local variable for testing rpm dir
  SOURCE="/srv/centreon-yum/yum.centreon.com/$PROJECT_PATH/$MAJOR/$DISTRIB/testing/$ARCH/$PROJECT/$FOLDER"
  #Local variable for stable rpm dir
  TARGET="/srv/centreon-yum/yum.centreon.com/$PROJECT_PATH/$MAJOR/$DISTRIB/stable/$ARCH/RPMS"
  #Local variable for metadatas updates
  METADATAS="/srv/centreon-yum/yum.centreon.com/$PROJECT_PATH/$MAJOR/$DISTRIB/stable/$ARCH"

  #Create directory and copy rpm from testing repo to the stable repo
  ssh -o StrictHostKeyChecking=no "cesync@yum.int.centreon.com" mkdir -p "$TARGET"
  ssh -o StrictHostKeyChecking=no "cesync@yum.int.centreon.com" cp $SOURCE/*.rpm $TARGET/

  #Update metadatas and invalidate cache on cloudfront
  ssh -o StrictHostKeyChecking=no "cesync@yum.int.centreon.com" createrepo "$METADATAS"
  ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" aws cloudfront create-invalidation --distribution-id E34EBWWERP6QET --paths "/$PROJECT_PATH/$MAJOR/$DISTRIB/stable/$ARCH/*"
}