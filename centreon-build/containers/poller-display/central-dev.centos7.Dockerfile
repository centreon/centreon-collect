# Base information.
FROM ci.int.centreon.com:5000/mon-poller-display-central:centos7
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Database.
COPY poller-display/install-central-dev.sh /tmp/install.sh
COPY centreon-poller-display-central/sql /usr/share/centreon/www/modules/centreon-poller-display-central/sql
RUN /tmp/install.sh

# Sources.
COPY centreon-poller-display-central/centreon-poller-display-central.conf.php /usr/share/centreon/www/modules/centreon-poller-display-central/centreon-poller-display-central.conf.php
COPY centreon-poller-display-central/generate_files /usr/share/centreon/www/modules/centreon-poller-display-central/generate_files
COPY centreon-poller-display-central/core /usr/share/centreon/www/modules/centreon-poller-display-central/core
