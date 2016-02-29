# Centreon Build : Containers

## Introduction.

Centreon continuous integration system uses Docker to provide fast
testing environments. Usually any image is provided with two tags :
**centos6** and **centos7**, according to the operating system flavor.

Images are hosted on a private registry hosted at
*ci.int.centreon.com:5000*. If you wish to use images provided by this
registry you need to configure Docker to explicit trust it. Add
**--insecure-registry ci.int.centreon.com:5000** to your Docker daemon
command line (either */lib/systemd/system/docker.service*,
*/etc/default/docker* or some similar file).

## mon-build-dependencies

These images contain downloaded build dependencies required to build
Centreon software. They are used to build RPMs with docker-rpm-builder.

## mon-dependencies

These images have all dependencies required to run Centreon installed.
They are not used directly but to build mon-unittest and mon-web images.

## mon-unittest

These images contain all software needed to run tests on Centreon
software. They are used to run unit tests.

## mon-web

These images contain Centreon already installed and ready to run. They
are used in the continuous integration system to run acceptance tests.

## mon-ppe

Has Centreon Plugin Pack Exporter module installed on top of Centreon
Web. Therefore it reuse the *mon-web* images.
