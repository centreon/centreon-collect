#!/bin/sh

set -e
set -x

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# Clean previous releases
$SSH_REPO rm -rf /srv/p2/license/
$SSH_REPO mkdir /srv/p2/license/

# Copy installer
INSTALLER_NAME="centreon-studio-decktop-client/com.centreon.studio.license.packaging.nsis.x86_64/target/*.exe"
scp -o StrictHostKeyChecking=no $INSTALLER_NAME "ubuntu@srvi-repo.int.centreon.com:/srv/p2/license/"

# Copy plugins for automatic update
$SSH_REPO mkdir /srv/p2/license/p2/
scp -o StrictHostKeyChecking=no -r centreon-studio-desktop-client/com.centreon.studio.license.product/target/repository/* "ubuntu@srvi-repo.int.centreon.com:/srv/p2/license/p2/"

