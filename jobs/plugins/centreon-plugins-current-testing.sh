#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Build RPMs.
docker pull ci.int.centreon.com:5000/centreon-plugins:latest
containerid=`docker create ci.int.centreon.com:5000/centreon-plugins:latest`
docker start -a "$containerid"

# Publish RPMs.
rm -rf noarch
docker cp "$containersid:/script/build/rpmbuild/RPMS/noarch" .
scp noarch/*.el6.*.rpm ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el6/testing/noarch/RPMS/
ssh ubuntu@srvi-repo.int.centreon.com createrepo /srv/yum/standard/3.4/el6/testing/noarch
scp noarch/*.el7.*.rpm ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el7/testing/noarch/RPMS/
ssh ubuntu@srvi-repo.int.centreon.com createrepo /srv/yum/standard/3.4/el7/testing/noarch

# Cleanup.
docker rm "$containerid"
