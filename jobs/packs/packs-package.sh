#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-packs

# Pull build dependencies images.
REGISTRY="ci.int.centreon.com:5000"
BUILD_IMG_CENTOS6="$REGISTRY/mon-build-dependencies-3.4:centos6"
BUILD_IMG_CENTOS7="$REGISTRY/mon-build-dependencies-18.10:centos7"
docker pull "$BUILD_IMG_CENTOS6"
docker pull "$BUILD_IMG_CENTOS7"

# Get project release.
cd centreon-plugin-packs
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
RELEASE="$now.$COMMIT"
cd ..

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos6
mkdir output-centos6
rm -rf output-centos7
mkdir output-centos7
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
    cp "centreon-plugin-packs/src/$pack/pack.json" "packs/$pack.json"
    sed -e "s/@PACK@/$pack/g" -e "s/@VERSION@/$newversion/g" -e "s/@OLDPACK@/$oldpack/g" < `dirname $0`/../../packaging/packs/pack.body.spectemplate >> input/centreon-packs.spec
  fi
done

# Build RPMs.
tar czf input/packs.tar.gz packs
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS6" input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS7" input output-centos7

# Remove 'fake' package.
rm -f output-centos6/noarch/centreon-pack-1.0.0*.rpm
rm -f output-centos7/noarch/centreon-pack-1.0.0*.rpm

# Process only if some packages were generated.
packagecount=`ls output-centos7/noarch | wc -l`
if [ "$packagecount" -gt 0 ] ; do
  put_internal_rpms "3.4" "el6" "noarch" "packs" "$PROJECT-$RELEASE" output-centos6/noarch/*.rpm
  put_internal_rpms "3.4" "el7" "noarch" "packs" "$PROJECT-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "18.10" "el7" "noarch" "packs" "$PROJECT-$RELEASE" output-centos7/noarch/*.rpm
  if [ "$BRANCH_NAME" '=' 'master' ] ; then
    copy_internal_rpms_to_unstable "plugin-packs" "3.4" "el6" "noarch" "packs" "$PROJECT-$RELEASE"
    copy_internal_rpms_to_unstable "plugin-packs" "3.4" "el7" "noarch" "packs" "$PROJECT-$RELEASE"
    copy_internal_rpms_to_canary "plugin-packs" "18.10" "el7" "noarch" "packs" "$PROJECT-$RELEASE"
  fi
fi
