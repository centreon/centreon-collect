#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Generate .repo and .spec.
rm -rf el7 el8
php `dirname $0`/../../../packaging/repo/21.04/genrepo.php

# Build all release RPMs.
for distrib in el7 el8 ; do
  for project in centreon centreon-bam centreon-map centreon-mbi centreon-plugin-packs ; do
    # Create temporary directories.
    rm -rf input output
    mkdir input
    mkdir output

    # Copy source files.
    cp `dirname $0`/../../../packaging/repo/RPM-GPG-KEY-CES input
    cp $distrib/$project.repo input
    cp $project.spec input

    # Build RPM.
    if [ "$distrib" = 'el7' ] ; then
      tag='centos7';
    elif [ "$distrib" = 'el8' ] ; then
      tag='centos8'
    else
      echo "Unsupported distribution $distrib"
      exit 1
    fi
    BUILD_IMG="registry.centreon.com/mon-build-dependencies-20.04:$tag"
    docker pull "$BUILD_IMG"
    docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output

    # Push RPM.
    if [ "$project" = centreon ] ; then
      REPO='standard'
    elif [ "$project" = 'centreon-bam' ] ; then
      REPO='bam'
    elif [ "$project" = 'centreon-map' ] ; then
      REPO='map'
    elif [ "$project" = 'centreon-mbi' ] ; then
      REPO='mbi'
    elif [ "$project" = 'centreon-plugin-packs' ] ; then
      REPO='plugin-packs'
    fi
    put_testing_rpms "$REPO" 21.04 "$distrib" noarch repo '' output/noarch/*.rpm
  done
done
