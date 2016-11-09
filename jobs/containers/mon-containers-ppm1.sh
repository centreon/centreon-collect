#!/bin/sh

set -e
set -x

DISTRIB="centos6"

# Pull Centreon dependencies.
docker pull ci.int.centreon.com:5000/mon-dependencies:$DISTRIB

# Prepare Dockerfile.
cd centreon-build/containers
sed "s/@DISTRIB@/$DISTRIB/g" < ppm/ppm1.Dockerfile.in > ppm/ppm1.$DISTRIB.Dockerfile

# CentOS PPM image.
rm -rf centreon-pp-manager
cp -r ../../centreon-import/www/modules/centreon-pp-manager .
docker build --no-cache -t ci.int.centreon.com:5000/mon-ppm1:$DISTRIB -f ppm/ppm1.$DISTRIB.Dockerfile .
docker push ci.int.centreon.com:5000/mon-ppm1:$DISTRIB
