#!/bin/sh

set -e
set -x

DISTRIB="centos6"

# Pull Centreon dependencies.
docker pull ci.int.centreon.com:5000/mon-dependencies:$DISTRIB

# Prepare Dockerfile.
cd centreon-build/containers
sed "s/@DISTRIB@/$DISTRIB/g" < ppe/ppe1.Dockerfile.in > ppe/ppe1.$DISTRIB.Dockerfile

# CentOS PPM image.
rm -rf centreon-export
cp -r ../../centreon-export/www/modules/centreon-export .
docker build --no-cache -t ci.int.centreon.com:5000/mon-ppe1:$DISTRIB -f ppe/ppe1.$DISTRIB.Dockerfile .
docker push ci.int.centreon.com:5000/mon-ppe1:$DISTRIB