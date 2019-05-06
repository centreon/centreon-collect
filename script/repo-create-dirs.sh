#!/bin/sh

# Base directory.
BASEDIR="/srv/yum"

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <serie>"
  exit 1
fi

# Create synchronized directories.
for project in bam map mbi plugin-packs standard ; do
  for channel in canary unstable testing stable ; do
    for arch in noarch x86_64 ; do
      targetdir="$BASEDIR/$project/$1/el7/$channel/$arch"
      mkdir -p "$targetdir"
      createrepo "$targetdir"
    done
  done
done

# Create internal directories.
for arch in noarch x86_64 ; do
  mkdir -p "$BASEDIR/internal/$1/el7/$arch"
done
