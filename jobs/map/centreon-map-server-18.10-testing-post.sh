#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-map-server
VERSION=18.10

# This is run once the Maven build terminated.

# Copy the generated RPM files into the internal RPM repository.

# Get the folder path of the generated RPMs.
TOMCAT7_PATH="server/map-server-parent/map-server-packaging/map-server-packaging-tomcat7/target/rpm/centreon-map-server/RPMS/noarch/"

# Get the name of each RPM files.
# This way of getting the RPM's names is not pretty but it works.
for filename in "$TOMCAT7_PATH"/*.rpm; do
    TOMCAT7_FILE=${filename##*/}
done

# Copy the RPMs into the remote internal RPM repository.
put_testing_rpms "map" "18.10" "el7" "noarch" "map-server" "$PROJECT-$VERSION-$RELEASE" $TOMCAT7_PATH/$TOMCAT7_FILE

# Sign RPMs on directly on the remote internal RPM repository.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO rpm --resign /srv/yum/map/18.10/el7/testing/noarch/map-server/$PROJECT-$VERSION-$RELEASE/$TOMCAT7_FILE

# update yum metadata so that the remote RPM repository will be up to date
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/map/18.10/el7/testing/noarch

# Generate doc on internal server.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-map-4 -V latest -p'"
