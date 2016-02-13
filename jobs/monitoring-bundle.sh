#!/bin/sh

# CentOS 6 running image.
cd centreon-build/containers/centos6
docker build -t monitoring-running:centos6 -f running.Dockerfile .
docker tag -f monitoring-running:centos6 10.24.11.199:5000/monitoring-running:centos6
docker push 10.24.11.199:5000/monitoring-running:centos6

# CentOS 7 running image.
cd ../centos7
docker build -t monitoring-running:centos7 -f running.Dockerfile .
docker tag -f monitoring-running:centos7 10.24.11.199:5000/monitoring-running:centos7
docker push 10.24.11.199:5000/monitoring-running:centos7
