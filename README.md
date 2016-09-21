# Centreon Build

## Introduction

Centreon Build project contains specific components needed to perform
continuous integration on Centreon software. The continuous integration
server used with these components is Jenkins. Everything that begins
with *mon-* is made by and for the Monitoring and Metrology Team of
Centreon R&D.

## How to run acceptance tests

If you're interested in how to run acceptance tests you might find the
[script directory documentation](script/README.md) useful.

## Architecture.

For each base component (Engine, Web, Broker, ...), there is a job
pipeline triggered after every commit to the current development branch
(usually master, but this can vary). These component-specific jobs are
usually as follow :

  * unittest : run unit tests
  * package : build RPMs and publish them on the internal repository, or
    create a tarball and upload it to a known location
  * bundle : build Docker images with components installed
  * acceptance : run acceptance tests on bundled images
  * release : (not yet implemented ; if all tests were successful,
    sources and packages can be uploaded online and made available to
    customers)

Each job is defined in a script that resides in the
[jobs](jobs/README.md) subfolder. This repository serves as reference
for these jobs that are executed using the master branch.

The [containers](containers/README.md) subfolder contains Dockerfile
and appropriate scripts to build containers used in the continuous
integration process.
