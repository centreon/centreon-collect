#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
WEB_IMAGE=ci.int.centreon.com:5000/mon-web:$DISTRIB
docker pull $WEB_IMAGE

# Fetch LM sources.
scp -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com:/tmp/centreon-license-manager.$DISTRIB.tar.gz" .
tar xzf centreon-license-manager.$DISTRIB.tar.gz

# Prepare Dockerfile.
cd centreon-build/containers/
sed "s/@DISTRIB@/$DISTRIB/g" < lm/lm.Dockerfile.in > lm/lm.$DISTRIB.Dockerfile

# Build image.
rm -rf centreon-license-manager
cp -R ../../centreon-license-manager/www/modules/centreon-license-manager .
docker build --no-cache -t ci.int.centreon.com:5000/mon-lm:$DISTRIB -f lm/lm.$DISTRIB.Dockerfile .
docker push ci.int.centreon.com:5000/mon-lm:$DISTRIB
