# Centreon Build : Containers

## Introduction

Centreon continuous integration system uses Docker to provide fast
testing environments. Usually any image is provided with two tags :
**centos6** and **centos7**, according to the operating system flavor.

Images are hosted on a private registry hosted at
*ci.int.centreon.com:5000*. If you wish to use images provided by this
registry you need to configure Docker to explicit trust it. Add
**--insecure-registry ci.int.centreon.com:5000** to your Docker daemon
command line (either */lib/systemd/system/docker.service*,
*/etc/default/docker* or some similar file).
You can also try to add the following line in the file */etc/docker/daemon.json* (create it if needed):
**{ "insecure-registries":["ci.int.centreon.com:5000"] }**

## Building images

Images can be built directly using *docker build*. The sole tricky part
is the context directory which must be set to *containers*. For example
if you wish to build the *mon-build-dependencies:centos6* image, you
could do it like so.

```shell
cd centreon-build/containers
docker build -t ci.int.centreon.com:5000/mon-build-dependencies:centos6 -f build-dependencies.centos6.Dockerfile .
```

Note the ending dot. This is directory context we're talking about.

## Image list

### mon-build-dependencies

These images contain downloaded build dependencies required to build
Centreon software. They are used to build RPMs with docker-rpm-builder.

### mon-dependencies

These images have all dependencies required to run Centreon installed.
They are not used directly but to build mon-unittest and mon-web images.

### mon-unittest

These images contain all software needed to run tests on Centreon
software. They are used to run unit tests.

### mon-middleware

This single image (:latest) hosts Centreon IMP Portal API which is used
notably by Centreon Plugin Pack Manager.

### mon-web-fresh

These images contain Centreon fresh installed and ready to run. They
are used in the continuous integration system to run some acceptance
tests.

### mon-web

These images contain a standard configuration on top of *mon-web-fresh*.
This is supposed to reflect what a standard installation is most of the
time.

### mon-ppe

Has Centreon Plugin Pack Exporter module installed on top of Centreon
Web. Therefore it reuses the *mon-web* images.

### mon-lm

Has Centreon License Manager module installed on top of Centreon Web.
Therefore it reuses the *mon-web* images.

### mon-ppm

Has Centreon Plugin Pack Manager and Centreon License Manager installed
on top of Centreon Web. It reuses *mon-web* images.

### mon-automation

Has Centreon Automation installed along Centreon Plugin Pack Manager and
Centreon License Manager. Therefore it reuses *mon-ppm* images.

### des-bam

Has Centreon BAM module on top of Centreon Web. It reuses *mon-web*
images.

### des-map-server

Has Centreon Map Server. Usually needs also des-map-web to work properly
and provide all Centreon Map features.

### des-map-web

Has correct configuration to work with des-map-server and has Centreon
Map Web Client installed on top of *mon-web* images.

### mon-squid-simple

Squid container to test proxy in Centreon.

### mon-squid-basic-auth

Squid container to test proxy with basic auth in Centreon.
Proxy user : proxy-user
Proxy-password : proxy-password
