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
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|...>"
  exit 1
fi
DISTRIB="$1"

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "plugins/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Pull latest build dependencies.
BUILD_IMG="ci.int.centreon.com:5000/mon-build-dependencies-18.10:$DISTRIB"
docker pull "$BUILD_IMG"

# Create cache directory.
rm -rf "cache-$VERSION-$RELEASE"
mkdir "cache-$VERSION-$RELEASE"

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get base spec file.
cp `dirname $0`/../../packaging/plugins/plugin.head.spectemplate input/plugin.spectemplate

# Process all packages.
atleastoneplugin=0
for package in `dirname $0`/../../packaging/plugins/centreon-plugin-* ; do
  package=`echo $package | rev | cut -d / -f 1 | rev`

  # Extract package information.
  pkgpath=`dirname $0`/../../packaging/plugins/$package/pkg.json
  NAME=`python -c "import sys, json; print json.load(sys.stdin)['pkg_name']" < $pkgpath`
  PLUGIN_NAME=`python -c "import sys, json; print json.load(sys.stdin)['plugin_name']" < $pkgpath`
  rpmpath=`dirname $0`/../../packaging/plugins/$package/rpm.json
  REQUIRES=`python -c "import sys, json; print ', '.join(json.load(sys.stdin).get('dependencies', ''))" < $rpmpath`
  CUSTOM_PKG_DATA=`python -c "import sys, json; print json.load(sys.stdin).get('custom_pkg_data', '')" < $rpmpath`
  export NAME
  export PLUGIN_NAME
  export REQUIRES
  export CUSTOM_PKG_DATA

  # Get current reference files.
  curl -o plugin.head.spectemplate "http://srvi-repo.int.centreon.com/cache/plugins/$package/plugin.head.spectemplate"
  curl -o plugin.body.spectemplate "http://srvi-repo.int.centreon.com/cache/plugins/$package/plugin.body.spectemplate"
  curl -o plugin.pl "http://srvi-repo.int.centreon.com/cache/plugins/$package/plugin.pl"
  curl -o pkg.json "http://srvi-repo.int.centreon.com/cache/plugins/$package/pkg.json"
  curl -o rpm.json "http://srvi-repo.int.centreon.com/cache/plugins/$package/rpm.json"

  # Build plugin only if current files are different.
  set +e
  headspecpath=`dirname $0`/../../packaging/plugins/plugin.head.spectemplate
  cmp "$headspecpath" plugin.head.spectemplate
  headspecdiff=$?
  bodyspecpath=`dirname $0`/../../packaging/plugins/plugin.body.spectemplate
  cmp "$bodyspecpath" plugin.body.spectemplate
  bodyspecdiff=$?
  cmp "$PROJECT-$VERSION/$PLUGIN_NAME" plugin.pl
  plugindiff=$?
  cmp $pkgpath pkg.json
  pkgdiff=$?
  cmp $rpmpath rpm.json
  rpmdiff=$?
  set -e
  if [ "$headspecdiff" -ne 0 -o "$bodyspecdiff" -ne 0 -o "$plugindiff" -ne 0 -o "$pkgdiff" -ne 0 -o "$rpmdiff" -ne 0 ] ; then
    atleastoneplugin=1

    # Copy plugin to input directory.
    cp "$PROJECT-$VERSION/$PLUGIN_NAME" input/

    # Append package to spectemplate.
    subpkgname=`echo $NAME | cut -d - -f 3-`
    sed -e "s#@NAME@#$subpkgname#g" -e "s#@PLUGIN_NAME@#$PLUGIN_NAME#g" -e "s#@REQUIRES@#$REQUIRES#g" -e "s#@CUSTOM_PKG_DATA@#$CUSTOM_PKG_DATA#g" < "$bodyspecpath" >> input/plugin.spectemplate

    # Populate cache with new files.
    cachedir="/srv/cache/plugins/internal/$VERSION-$RELEASE/$package"
    mkdir "$cachedir"
    cp "$headspecpath" "$cachedir/"
    cp "$bodyspecpath" "$cachedir/"
    cp "$PROJECT-$VERSION/$PLUGIN_NAME" "$cachedir/"
    cp "$pkgpath" "$cachedir/"
    cp "$rpmpath" "$cachedir/"
  fi
done

if [ "$atleastoneplugin" -ne 0 ] ; then
  # Build RPMs.
  docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG" input output
  rm -f output/noarch/centreon-plugin-$VERSION-*

  # Copy files to server.
  put_internal_rpms "18.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
  if [ "$BRANCH_NAME" '=' 'master' ] ; then
    copy_internal_rpms_to_canary "standard" "18.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
  fi

  # Populate cache.
  scp -r "cache-$VERSION-$RELEASE" "$REPO_CREDS:/var/cache/plugins/internal/"
fi
