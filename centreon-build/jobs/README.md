# Centreon Build : Jobs

## Unit tests jobs

### monitoring-engine-unittest.sh

Build instrumented Centreon Engine binaries and run unit tests in
monitoring-unittest images. Report unit test results.

### monitoring-broker-unittest.sh

Build instrumented Centreon Broker binaries and run unit tests in
monitoring-unittest images. Report unit test results.

### monitoring-web-unittest.sh

To be done. Will run Centreon Web unit tests and report test results.

## Packaging jobs

### monitoring-engine-package.sh

Run docker-rpm-builder in monitoring-build-dependencies images.
Generated RPMs are then pushed to the internal repository
(srvi-ces-repository, branch testing).

### monitoring-broker-package.sh

Run docker-rpm-builder in monitoring-build-dependencies images.
Generated RPMs are then pushed to the internal repository
(srvi-ces-repository, branch testing).

### monitoring-web-package.sh

Run docker-rpm-builder in monitoring-build-dependencies images.
Generated RPMs are then pushed to the internal repository
(srvi-ces-repository, branch testing).

## Global jobs

### monitoring-bundle.sh

Bundle all software pieces together. It mainly consists of installing
RPMs and running web installation steps to generate Docker images
(monitoring-running). These images will be used in the
*monitoring-acceptance* job.

### monitoring-acceptance.sh

To be done. Run acceptance tests with Behat on software components that
are available in Docker images (monitoring-running). When finished,
report test results.
