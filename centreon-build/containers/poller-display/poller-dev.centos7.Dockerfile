# Base information.
FROM ci.int.centreon.com:5000/mon-poller-display:centos7
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Database.
COPY poller-display/install-poller-dev.sh /tmp/install.sh
COPY centreon-poller-display/sql /usr/share/centreon/www/modules/centreon-poller-display/sql
RUN /tmp/install.sh

# Sources.
COPY centreon-poller-display/cron/centreon-poller-display-sync.sh /usr/share/centreon/cron/
COPY centreon-poller-display/core /usr/share/centreon/www/modules/centreon-poller-display/core
