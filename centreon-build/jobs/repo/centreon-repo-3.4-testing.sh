#!/bin/sh

set -e
set -x

# Pull build images.
docker pull registry.centreon.com/mon-build-dependencies-3.4:centos7

# Generate .repo and .spec.
rm -rf el7
php `dirname $0`/../../packaging/repo/genrepo-3.4.php

# Build all release RPMs.
distrib="el7"
for project in centreon centreon-bam centreon-map centreon-mbi centreon-plugin-packs ; do
  # Create temporary directories.
  rm -rf input output
  mkdir input
  mkdir output

  # Copy source files.
  cp `dirname $0`/../../packaging/repo/RPM-GPG-KEY-CES input
  cp $distrib/$project.repo input
  cp $project.spec input

  # Build RPM.
  if [ "$distrib" = 'el7' ] ; then
    tag='centos7';
  else
    echo "Unsupported distribution $distrib"
    exit 1
  fi
  docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-3.4:$tag input output

  # Push RPM.
  if [ "$project" = centreon ] ; then
    REPO='lts'
  elif [ "$project" = 'centreon-bam' ] ; then
    REPO='bam'
  elif [ "$project" = 'centreon-map' ] ; then
    REPO='map'
  elif [ "$project" = 'centreon-mbi' ] ; then
    REPO='mbi'
  elif [ "$project" = 'centreon-plugin-packs' ] ; then
    REPO='plugin-packs'
  fi
  REPO="$REPO/3.4/$distrib/testing/noarch"
  scp -o StrictHostKeyChecking=no output/noarch/*.rpm "ubuntu@srvi-repo.int.centreon.com:/srv/yum/$REPO/RPMS"
  DESTFILE=`ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mktemp`
  scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "ubuntu@srvi-repo.int.centreon.com:$DESTFILE"
  ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" sh $DESTFILE $REPO
done
