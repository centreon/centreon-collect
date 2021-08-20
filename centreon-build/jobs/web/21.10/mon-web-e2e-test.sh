#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 2 ] ; then
  echo "USAGE: $0 <centos7|centos8> [feature]"
  exit 1
fi
DISTRIB="$1"

# Pull images.
WEB_IMAGE="$REGISTRY/mon-web-$VERSION-$RELEASE:$DISTRIB"
docker pull $WEB_IMAGE

# Fetch sources.
rm -rf "$PROJECT-$VERSION"
tar xzf "$PROJECT-$VERSION.tar.gz"
chmod g+s "$PROJECT-$VERSION/tests/e2e"

# Unstash cypress dependencies
cd "$PROJECT-$VERSION/tests/e2e"
tar xzf "../../../cypress-node-modules.tar.gz"

# Prepare Docker Compose file.
path_feature="${2}"
FEATURE_FILE=`echo "${2}" | sed -e "s#^tests/e2e/##"`

# Get the current user ID for use in the Cypress.io image docker.
#Otherwise it creates result files as "root:root" and impossible to delete them.
UID="root"

# Prepare Docker Compose file.
sed 's#@WEB_IMAGE@#'$WEB_IMAGE'#g; s#@UID@#'$UID'#g; s#@FEATURE_FILE@#'$FEATURE_FILE'#g' < docker-compose.yml.in > docker-compose.yml

docker-compose pull
docker-compose up --abort-on-container-exit --exit-code-from cypress
