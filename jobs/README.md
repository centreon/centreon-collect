# Centreon Build : Jobs

## Unit tests jobs

### mon-engine-unittest.sh

Build instrumented Centreon Engine binaries and run unit tests in
mon-unittest images. Report unit test results.

### mon-broker-unittest.sh

Build instrumented Centreon Broker binaries and run unit tests in
mon-unittest images. Report unit test results.

### mon-web-unittest.sh

To be done. Will run Centreon Web unit tests and report test results.

### mon-lm-unittest.sh

Run Centreon License Manager unit tests and report test results.

### mon-ppe-unittest.sh

Run Centreon Plugin Pack Exporter unit tests and report test results.

## Packaging jobs

### mon-engine-package.sh

Run docker-rpm-builder in monitoring-build-dependencies images.
Generated RPMs are then pushed to the internal repository
(srvi-ces-repository, branch testing).

### mon-broker-package.sh

Run docker-rpm-builder in monitoring-build-dependencies images.
Generated RPMs are then pushed to the internal repository
(srvi-ces-repository, branch testing).

### mon-web-package.sh

Run docker-rpm-builder in monitoring-build-dependencies images.
Generated RPMs are then pushed to the internal repository
(srvi-ces-repository, branch testing).

### mon-lm-package.sh

Build a tarball of Centreon License Manager and upload it to
srvi-ces-repository in /tmp.

### mon-ppe-package.sh

Build a tarball of Centreon Plugin Pack Exporter and upload it to
srvi-ces-repository in /tmp.

## Bundle jobs

### mon-web-bundle.sh

Bundle core software pieces together. It mainly consists of installing
RPMs of core components (Centreon Engine, Centreon Broker and Centreon
Web) and running web installation steps to generate Docker images
(mon-web). These images will be used in the *mon-web-acceptance* job.

### mon-lm-bundle.sh

Install Centreon License Manager to the *mon-lm* images by using the
*mon-web* images.

### mon-ppe-bundle.sh

Install Centreon Plugin Pack Exporter to the *mon-ppe* images by using
the *mon-web* images. This includes source copy and web-install (well
a custom helper script in fact).

## Acceptance jobs

### mon-web-acceptance.sh

Run acceptance tests with Behat on core software components that are
installed in the *mon-web* images. When finished, report test results.

### mon-ppe-acceptance.sh

Run acceptance tests with Behat on Centreon Plugin Pack Exporter
(*mon-ppe* images). When finished, report test results.
