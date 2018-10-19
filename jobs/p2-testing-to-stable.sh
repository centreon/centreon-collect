#!/bin/sh

# Check arguments.
if [ -z "$VERSION" ] ; then
  echo "You need to specify VERSION environment variable."
  exit 1
fi

export MAJOR=`echo $VERSION | cut -d . -f 1`
export MINOR=`echo $VERSION | cut -d . -f 2`
export BUGFIX=`echo $VERSION | cut -d . -f 3`

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO ssh -o StrictHostKeyChecking=no "map-repo@10.24.1.107" rm -rf "centreon-studio-repository/$MAJOR/$MINOR"
$SSH_REPO scp -r "/srv/p2/testing/$MAJOR/$MINOR" "map-repo@10.24.1.107:centreon-studio-repository/$MAJOR/$MINOR"
