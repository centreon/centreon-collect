#!/bin/sh

set -e
set -x

# Pull build images.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos6
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos7

# Generate .repo and .spec.
rm -rf el6 el7
php `dirname $0`/../../packaging/repo/genrepo.php

# Build all release RPMs.
for distrib in el6 el7 ; do
  for project in '' bam map mbi packs ; do
    # Create temporary directories.
    rm -rf input output
    mkdir input
    mkdir output

    # Copy source files.
    cp `dirname $0`/../../packaging/repo/RPM-GPG-KEY-CES input
    cp $distrib/$project.repo input
    cp $project.spec input

    # Build RPM.
    if [ "$distrib" = 'el6' ] ; then
      tag='centos6';
    elif [ "$distrib" = 'el7' ] ; then
      tag='centos7';
    else
      echo "Unsupported distribution $distrib"
      exit 1
    fi
    docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:$tag input output

    # Push RPM.
    if [ "$project" = '' ] ; then
      REPO="standard/3.4/$distrib/testing/noarch"
    elif [ "$project" = 'bam' -o "$project" = 'map' -o "$project" = 'mbi' ] ; then
      REPO="$project/3.4/$distrib/testing/noarch"
    elif [ "$project" = 'packs' ] ; then
      REPO="plugin-packs/3.4/$distrib/testing/noarch"
    fi
    scp -o StrictHostKeyChecking=no output/noarch/*.rpm "ubuntu@srvi-repo.int.centreon.com:/srv/yum/$REPO/RPMS"
    DESTFILE=`ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mktemp`
    scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "ubuntu@srvi-repo.int.centreon.com:$DESTFILE"
    ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" sh $DESTFILE $REPO
  done
done
