#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-packs

# Pull build dependencies images.
REGISTRY="registry.centreon.com"
BUILD_IMG_CENTOS6="$REGISTRY/mon-build-dependencies-3.4:centos6"
BUILD_IMG_CENTOS7="$REGISTRY/mon-build-dependencies-19.04:centos7"
BUILD_IMG_CENTOS8="$REGISTRY/mon-build-dependencies-20.10:centos8"
docker pull "$BUILD_IMG_CENTOS6"
docker pull "$BUILD_IMG_CENTOS7"
docker pull "$BUILD_IMG_CENTOS8"

# Generate version and release.
VERSION=`date '+%Y%m%d'`
RELEASE=`date '+%H%M%S'`

# Create cache directory.
rm -rf "cache-$VERSION-$RELEASE"
mkdir "cache-$VERSION-$RELEASE"

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos6
mkdir output-centos6
rm -rf output-centos7
mkdir output-centos7
rm -rf output-centos8
mkdir output-centos8
cp `dirname $0`/../../packaging/packs/pack.head.spectemplate input/centreon-packs.spec

# Get packs that changed.
rm -rf packs
mkdir packs
for pack in `ls centreon-plugin-packs/src` ; do
  curl -o base.json "http://srvi-repo.int.centreon.com/cache/packs/stable/$pack/pack.json"
  set +e
  baseversion=`python -c "import sys, json; print json.load(sys.stdin)['information']['version']" < base.json`
  set -e
  newversion=`python -c "import sys, json; print json.load(sys.stdin)['information']['version']" < centreon-plugin-packs/src/$pack/pack.json`
  if [ "$baseversion" '!=' "$newversion" ] ; then
    oldpack=`echo "$pack" | sed -e 's/^\([a-z]\)/\U\1/g' -e 's/-\([a-z]\)/-\U\1/g'`
    cp "centreon-plugin-packs/src/$pack/pack.json" "packs/pluginpack_$pack-$newversion.json"
    sed -e "s/@PACK@/$pack/g" -e "s/@VERSION@/$newversion/g" -e "s/@OLDPACK@/$oldpack/g" < `dirname $0`/../../packaging/packs/pack.body.spectemplate >> input/centreon-packs.spec
    cp -r "centreon-plugin-packs/src/$pack" "cache-$VERSION-$RELEASE/"
  fi
done

# Process only if some packages are to be generated.
packagecount=`ls packs | wc -l`
if [ "$packagecount" -gt 0 ] ; then
  # Generate source tarballs.
  tar czf input/packs.tar.gz packs
  rm -rf "$PROJECT-$VERSION"
  mv packs "$PROJECT-$VERSION"
  tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
  put_internal_source "packs" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"

  # Build RPMs.
  docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS6" input output-centos6
  docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS7" input output-centos7
  docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS8" input output-centos8

  # Remove 'fake' package.
  rm -f output-centos6/noarch/centreon-pack-1.0.0*.rpm
  rm -f output-centos7/noarch/centreon-pack-1.0.0*.rpm
  rm -f output-centos8/noarch/centreon-pack-1.0.0*.rpm

  # Publish RPMs.
  put_internal_rpms "3.4" "el6" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE" output-centos6/noarch/*.rpm
  put_internal_rpms "3.4" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "19.04" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "19.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "20.04" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "20.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "20.10" "el8" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE" output-centos8/noarch/*.rpm
  if [ "$BRANCH_NAME" '=' 'master' ] ; then
    copy_internal_rpms_to_unstable "plugin-packs" "3.4" "el6" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "plugin-packs" "3.4" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "plugin-packs" "19.04" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "plugin-packs" "19.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "plugin-packs" "20.04" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "plugin-packs" "20.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "plugin-packs" "20.10" "el8" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
  fi

  # Populate cache.
  scp -r "cache-$VERSION-$RELEASE" "$REPO_CREDS:/srv/cache/packs/unstable"
fi
