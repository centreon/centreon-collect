#!/bin/sh

# Pull monitoring-dependencies image.
docker pull 10.24.11.199:5000/monitoring-dependencies:centos6
docker tag -f 10.24.11.199:5000/monitoring-dependencies:centos6 monitoring-dependencies:centos6

# CentOS 6 running image.
cd centreon-build/containers/centos6
docker build -t monitoring-running:centos6 -f running.Dockerfile .
docker tag -f monitoring-running:centos6 10.24.11.199:5000/monitoring-running:centos6
docker push 10.24.11.199:5000/monitoring-running:centos6

# Pull monitoring-dependencies images.
docker pull 10.24.11.199:5000/monitoring-dependencies:centos7
docker tag -f 10.24.11.199:5000/monitoring-dependencies:centos7 monitoring-dependencies:centos7

# CentOS 7 running image.
cd ../centos7
docker build -t monitoring-running:centos7 -f running.Dockerfile .
docker tag -f monitoring-running:centos7 10.24.11.199:5000/monitoring-running:centos7
docker push 10.24.11.199:5000/monitoring-running:centos7
