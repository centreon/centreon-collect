# Centreon Build : Scripts

## Quick use guide

* Docker must be installed on your machine

```shell
$# yum install docker-engine
$# usermod -a -G docker myuser
```

* You should have cloned the *centreon-build* directory somewhere

```shell
$> git clone https://github.com/centreon/centreon-build
```

* You should have run *acceptance.php -s* at least once

```shell
$> php /path/to/acceptance.php -s
```

* *acceptance.php* must be run from the root of your project directory

```shell
$> cd /path/to/centreon-bam
```

* You should have installed project dependencies

```shell
$> composer install
```

* The defaults of *acceptance.php* are to run all acceptance tests
  against local sources on CentOS 6

```shell
$> php /path/to/acceptance.php
```

## Flags

The flags of *acceptance.php* are defined below.

### -g Generate only

Only generate images, do not run tests. This can be useful for manual
testing.

### -d Distribution

Set the Linux distribution with which tests should be run. This can be *centos7*.

### -c Continuous integration

Use images from the continuous integration, not locally generated
images. Can be used when tests work with locally generated images
but not on the CI platform.

### -s Synchronize

Fetch up-to-date images from the registry server.

## How-to...

### ... run a container with my branch sources

```shell
$> cd /path/to/centreon-web
$> php /path/to/acceptance.php -g
$> docker-compose -f mon-web-dev up -d
$> docker ps

... working with the container at http://localhost:34242 or something (checkout docker ps output)...

$> docker-compose -f mon-web-dev.yml down
```

### ... run a CI container

```shell
$> cd /path/to/centreon-web
$> php /path/to/acceptance.php -g -c
$> docker-compose -f mon-web-dev up -d
$> docker ps

... working with the container at http://localhost:34242 or something (checkout docker ps output)...

$> docker-compose -f mon-web-dev.yml down
```

### ... debug within a container

First run a container (see above).

```shell
$> docker ps
CONTAINER ID        IMAGE                                           ...
b6a3a6de9be2        registry.centreon.com/mon-phantomjs:latest   ...
ca73d70955ea        registry.centreon.com/mon-web:centos7        ...

$> docker exec -ti ca73d70955ea bash
$> export TERM=xterm

From now on you are within the container. You can run mysql, tailf some
logs, ...

$> exit
```

## Rationale

*acceptance.php* is made to easily run acceptance tests of Centreon
products. Acceptance tests use Docker containers to ensure that tests
are properly isolated. Therefore to use local in-dev sources in
containers it is required to build new images. That's the main feature
of *acceptance.php*.

At Centreon, acceptance tests use Behat and are written in PHP. They
resides in the *features* subdirectory of every product. Basically when
a single test is run through Behat, the following steps execute :

* through a clause like (*Given a Centreon server*), Behat launches one
  or multiple containers with Docker Compose. It is usually a local
  image built by *acceptance.php* (see below) but it can also be the
  reference product image hosted on ci.int.centreon.com when running on
  the continuous integration platform
* then the scenario executes. Usually Mink interacts with the
  application under test by clicking on it, setting fields, ...
* test result is printed

In this test context, *acceptance.php* helps mostly to build a local
image with the sources being developed. It performs four differents
steps to achieve this goal.

* configuration loading : load basic configuration and check which
  product it is currently targeting by looking up directory name
* prepare for execution : prepare Docker Compose files that will be
  used in acceptance tests to run and link containers together
* build development container : use a custom Dockerfile hosted in
  centreon-build to build a Docker image with current sources. Images
  will usually be named like mon-xxx-dev
* run acceptance tests : finally run Behat, directly on host machine
  (that is ./vendor/bin/behat)
