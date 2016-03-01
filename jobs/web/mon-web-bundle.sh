#!/bin/sh

# Pull mon-dependencies image.
docker pull ci.int.centreon.com:5000/mon-dependencies:centos6

# CentOS 6 main image.
cd centreon-build/containers/web
docker build -t ci.int.centreon.com:5000/mon-web:centos6 -f web.centos6.Dockerfile .
docker push ci.int.centreon.com:5000/mon-web:centos6

# Pull mon-dependencies images.
#docker pull ci.int.centreon.com:5000/mon-dependencies:centos7

# CentOS 7 main image.
#cd ../centos7
#docker build -t ci.int.centreon.com:5000/mon-web:centos7 -f web.centos7.Dockerfile .
#docker push ci.int.centreon.com:5000/mon-web:centos7
