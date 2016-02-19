# Centreon Build

## Introduction

Centreon Build project contains specific components needed to perform
continuous integration on Centreon software. The continuous integration
server used with these components is Jenkins. Everything that begins
with *monitoring-* is made by and for the Monitoring and Metrology Team
of Centreon R&D.

## Architecture.

For each base component (Engine, Web, Broker, ...), there is a job
pipeline triggered after every commit to the current development branch
(usually master). These component-specific jobs are usually as follow :

  * unittest : run unit tests
  * package : build RPMs and publish them on the internal repository

After any component has been packaged, some global jobs are run. They
are not specific to a single component but interact on all of them.

  * bundle : build Docker images with components installed
  * acceptance : run acceptance tests on bundled images
  * release : (not yet implemented ; if all tests were successful,
    sources and packages can be uploaded online and made available)

Each job is defined in a script that resides in the
[jobs](jobs/README.md) subfolder. This repository serves as reference
for these jobs that must be manually copied to the master Jenkins
instance.

The [containers](containers/README.md) subfolder contains Dockerfile
and appropriate scripts to build containers used in the continuous
integration process.
