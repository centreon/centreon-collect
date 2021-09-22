#!/bin/sh

set -e
set -x

# Pull base image.
docker pull osixia/openldap:latest

# Build light image
cd centreon-build/containers
docker build -t registry.centreon.com/mon-openldap:latest -f openldap/openldap.Dockerfile .

# Push light image
docker push registry.centreon.com/mon-openldap:latest

# Build image with dataset (lot of users)
cd openldap/bootstrap/ldif
max=1000
for i in `seq 6 $max`
do
    cat 07-user.ldif.in | sed "s/@NAME@/centreon-ldap$i/g" | sed "s/@UID@/$i/g" >> 07-user.ldif
done

cd ../../..
docker build -t registry.centreon.com/mon-openldap-dataset:latest -f openldap/openldap.Dockerfile .

# Push image with dataset
docker push registry.centreon.com/mon-openldap-dataset:latest
