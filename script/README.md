# Centreon Build : Scripts

## acceptance.php

### Rationale

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

### acceptance.php

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
