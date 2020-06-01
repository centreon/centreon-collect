#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-plugins

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "plugins/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Pull latest build dependencies.
REGISTRY="registry.centreon.com"
BUILD_IMG_CENTOS6="$REGISTRY/mon-build-dependencies-3.4:centos6"
BUILD_IMG_CENTOS7="$REGISTRY/mon-build-dependencies-19.04:centos7"
BUILD_IMG_CENTOS8="$REGISTRY/mon-build-dependencies-20.04:centos8"
docker pull "$BUILD_IMG_CENTOS6"
docker pull "$BUILD_IMG_CENTOS7"
docker pull "$BUILD_IMG_CENTOS8"

# Create cache directory.
rm -rf "cache-$VERSION-$RELEASE"
mkdir "cache-$VERSION-$RELEASE"

# Create input and output directories.
rm -rf input
mkdir -p "input/centreon-plugin-$VERSION"
rm -rf output-centos6
mkdir output-centos6
rm -rf output-centos7
mkdir output-centos7
rm -rf output-centos8
mkdir output-centos8

# Get base spec file.
cp `dirname $0`/../../packaging/plugins/plugin.head.spectemplate input/plugin.spectemplate

# Process all packages.
atleastoneplugin=0
for package in `dirname $0`/../../packaging/plugins/centreon-plugin-* ; do
  package=`echo $package | rev | cut -d / -f 1 | rev`

  # Extract package information.
  pkgpath=`dirname $0`/../../packaging/plugins/$package/pkg.json
  NAME=`python -c "import sys, json; print json.load(sys.stdin)['pkg_name']" < $pkgpath`
  SUMMARY=`python -c "import sys, json; print json.load(sys.stdin)['pkg_summary']" < $pkgpath`
  PLUGIN_NAME=`python -c "import sys, json; print json.load(sys.stdin)['plugin_name']" < $pkgpath`
  rpmpath=`dirname $0`/../../packaging/plugins/$package/rpm.json
  REQUIRES=`python -c "import sys, json; print ', '.join(json.load(sys.stdin).get('dependencies', ''))" < $rpmpath`
  CUSTOM_PKG_DATA=`python -c "import sys, json; print json.load(sys.stdin).get('custom_pkg_data', '')" < $rpmpath`
  export NAME
  export SUMMARY
  export PLUGIN_NAME
  export REQUIRES
  export CUSTOM_PKG_DATA

  # Get current reference files.
  curl -o plugin.head.spectemplate "http://srvi-repo.int.centreon.com/cache/plugins/stable/$package/plugin.head.spectemplate"
  curl -o plugin.body.spectemplate "http://srvi-repo.int.centreon.com/cache/plugins/stable/$package/plugin.body.spectemplate"
  curl -o plugin.pl "http://srvi-repo.int.centreon.com/cache/plugins/stable/$package/plugin.pl"
  curl -o pkg.json "http://srvi-repo.int.centreon.com/cache/plugins/stable/$package/pkg.json"
  curl -o rpm.json "http://srvi-repo.int.centreon.com/cache/plugins/stable/$package/rpm.json"

  # Build plugin only if current files are different.
  set +e
  headspecpath=`dirname $0`/../../packaging/plugins/plugin.head.spectemplate
  cmp "$headspecpath" plugin.head.spectemplate
  headspecdiff=$?
  bodyspecpath=`dirname $0`/../../packaging/plugins/plugin.body.spectemplate
  cmp "$bodyspecpath" plugin.body.spectemplate
  bodyspecdiff=$?
  # plugindiff will be compared to 1, as the $global_version variable
  # in the fatpack will always be different between two builds.
  plugindiff=`diff --suppress-common-lines --side-by-side "$PROJECT-$VERSION/$PLUGIN_NAME" plugin.pl | wc -l`
  cmp $pkgpath pkg.json
  pkgdiff=$?
  cmp $rpmpath rpm.json
  rpmdiff=$?
  set -e
  if [ "$headspecdiff" -ne 0 -o "$bodyspecdiff" -ne 0 -o "$plugindiff" -gt 1 -o "$pkgdiff" -ne 0 -o "$rpmdiff" -ne 0 ] ; then
    atleastoneplugin=1

    # Copy plugin to input directory.
    cp "$PROJECT-$VERSION/$PLUGIN_NAME" "input/centreon-plugin-$VERSION"

    # Append package to spectemplate.
    subpkgname=`echo $NAME | cut -d - -f 3-`
    sed -e "s#@NAME@#$subpkgname#g" -e "s#@SUMMARY@#$SUMMARY#g" -e "s#@PLUGIN_NAME@#$PLUGIN_NAME#g" -e "s#@REQUIRES@#$REQUIRES#g" -e "s#@CUSTOM_PKG_DATA@#$CUSTOM_PKG_DATA#g" < "$bodyspecpath" >> input/plugin.spectemplate

    # Populate cache with new files.
    cachedir="cache-$VERSION-$RELEASE/$package"
    mkdir "$cachedir"
    cp "$headspecpath" "$cachedir/"
    cp "$bodyspecpath" "$cachedir/"
    cp "$PROJECT-$VERSION/$PLUGIN_NAME" "$cachedir/plugin.pl"
    cp "$pkgpath" "$cachedir/"
    cp "$rpmpath" "$cachedir/"
  fi
done

if [ "$atleastoneplugin" -ne 0 ] ; then
  # Create source tarball.
  cd input
  tar czf "centreon-plugin-$VERSION.tar.gz" "centreon-plugin-$VERSION"
  cd ..

  # Build RPMs.
  docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS6" input output-centos6
  rm -f output-centos6/noarch/centreon-plugin-$VERSION-*
  docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS7" input output-centos7
  rm -f output-centos7/noarch/centreon-plugin-$VERSION-*
  docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS8" input output-centos8
  rm -f output-centos8/noarch/centreon-plugin-$VERSION-*

  # Copy files to server.
  put_internal_rpms "3.4" "el6" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" output-centos6/noarch/*.rpm
  put_internal_rpms "3.4" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "19.04" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "19.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "20.04" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "20.04" "el8" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" output-centos8/noarch/*.rpm
  put_internal_rpms "20.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
  put_internal_rpms "20.10" "el8" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" output-centos8/noarch/*.rpm
  if [ "$BRANCH_NAME" '=' 'master' ] ; then
    copy_internal_rpms_to_unstable "standard" "3.4" "el6" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "standard" "3.4" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "standard" "19.04" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "standard" "19.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "standard" "20.04" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "standard" "20.04" "el8" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "standard" "20.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
    copy_internal_rpms_to_unstable "standard" "20.10" "el8" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
  fi

  # Populate cache.
  scp -r "cache-$VERSION-$RELEASE" "$REPO_CREDS:/srv/cache/plugins/unstable/"
  clean_directory /srv/cache/plugins/unstable
fi
