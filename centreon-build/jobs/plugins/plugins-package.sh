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

# Process all packages.
for package in `dirname $0`/../../packaging/plugins/centreon-plugin-* ; do
  package=`echo $package | rev | cut -d / -f 1 | rev`

  # Create input and output directories.
  rm -rf input
  mkdir input
  rm -rf output
  mkdir output

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

  # Get spec file.
  cp `dirname $0`/../../packaging/plugins/plugin.spectemplate input/

  # Get current reference files.
  curl -o plugin.spectemplate "http://srvi-repo.int.centreon.com/cache/plugins/$package/plugin.spectemplate"
  curl -o plugin.pl "http://srvi-repo.int.centreon.com/cache/plugins/$package/plugin.pl"
  curl -o pkg.json "http://srvi-repo.int.centreon.com/cache/plugins/$package/pkg.json"
  curl -o rpm.json "http://srvi-repo.int.centreon.com/cache/plugins/$package/rpm.json"

  # Build plugin only if current files are different.
  set +e
  cmp input/plugin.spectemplate plugin.spectemplate
  specdiff=$?
  cmp "$PROJECT-$VERSION/$PLUGIN_NAME" plugin.pl
  plugindiff=$?
  cmp $pkgpath pkg.json
  pkgdiff=$?
  cmp $rpmpath rpm.json
  rmpdiff=$?
  set -e
  if [ "$specdiff" -ne 0 -o "$plugindiff" -ne 0 -o "$pkgdiff" -ne 0 -o "$rpmdiff" -ne 0 ] ; then
    # Build RPMs.
    cp "$PROJECT-$VERSION/$PLUGIN_NAME" input/
    docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output

    # Copy files to server.
    if [ "$DISTRIB" = 'centos7' ] ; then
      DISTRIB='el7'
    else
      echo "Unsupported distribution $DISTRIB."
      exit 1
    fi
    put_internal_rpms "18.10" "$DISTRIB" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
    if [ "$BRANCH_NAME" '=' 'master' ] ; then
      copy_internal_rpms_to_canary "standard" "18.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
    fi

    # Populate cache with new files.
    ssh "$REPO_CREDS" mkdir -p "/srv/cache/plugins/$package" || true
    scp input/plugin.spectemplate "$REPO_CREDS:/srv/cache/plugins/$package/plugin.spectemplate"
    scp "$PROJECT-$VERSION/$PLUGIN_NAME" "$REPO_CREDS:/srv/cache/plugins/$package/plugin.pl"
    scp "$pkgpath" "$REPO_CREDS:/srv/cache/plugins/$package/pkg.json"
    scp "$rpmpath" "$REPO_CREDS:/srv/cache/plugins/$package/rpm.json"
  fi
done
