# Centreon Build : Containers

## Introduction.

Centreon continuous integration system uses Docker to provide fast
testing environments. Usually any image is provided with two tags :
**centos6** and **centos7**, according to the operating system flavor.

## monitoring-build-dependencies

These images contain downloaded build dependencies required to build
Centreon software. They are used to build RPMs with docker-rpm-builder.

## monitoring-dependencies

These images have all dependencies required to run Centreon installed.
They are not used directly but to build monitoring-unittest and
monitoring-running images.

## monitoring-unittest

These images contain all software needed to run tests on Centreon
software. They are used to run unit tests.

## monitoring-running

These images contain Centreon already installed and ready to run. They
are used in the continuous integration system to run acceptance tests.
