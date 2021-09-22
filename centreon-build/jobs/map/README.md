# Centreon Map Build Scripts

There are 2 jobs per projects:

- testing: This job build the project from the sources, run the tests, eventually creates a docker image and deploy the created RPM in the testing folder of Centreon's repository.
For the desktop application, it copies the installer (.exe, .deb and .tar) in our internal server.

The internal server on which everything is copied after being build is this one:
http://srvi-repo.int.centreon.com

- stable: Move the RPM which are currently in the testing folder in the stable folder of Centreon's internal repository.


## Centreon Map Server

### Testing

There are 2 scripts:
- centreon-map-server-current-testing-pre.sh: It's executed at the beginning of the testing job.
- centreon-map-server-current-testing-post.sh: It's executed at the end of the testing job.

Between the 2 scripts, the project is build with the maven goals "clean install".

### Stable

There is one scipt:
- centreon-map-server-current-stable.sh.

## Centreon Map Desktop Client

### Testing

There are 2 scipts:
- centreon-map-client-current-testing-pre.sh: It's executed at the beginning of the testing job.
- centreon-map-client-current-testing-post.sh: It's executed at the end of the testing job.

Between the 2 scripts, the project is build with the maven goals "clean install".

### Stable

There is one script:
- centreon-map-client-current-stable.sh


## Centreon Map Web Client

### Testing

There is one script:
- centreon-map-web-current-testing.sh

### Stable

There is one script:
- centreon-map-web-current-stable.sh