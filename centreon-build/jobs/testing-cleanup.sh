#!/bin/sh

set -e
set -x

# SSH command.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# Check arguments.
if [ -z "$REPO" ] ; then
  echo "You need to specify REPO environment variable."
  exit 1
fi

# Clean REPO.
for distrib in el6 el7 ; do
  for arch in noarch x86_64 ; do
    $SSH_REPO rm -f "/srv/yum/$REPO/3.4/$distrib/testing/$arch/RPMS/*.rpm"
    $SSH_REPO createrepo "/srv/yum/$REPO/3.4/$distrib/testing/$arch"
  done
done
