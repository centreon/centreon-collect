# Base information.
FROM ci.int.centreon.com:5000/mon-dependencies:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Install Centreon Web.
RUN yum install -y centreon-base-config-centreon-engine mysql-server git
RUN echo 'date.timezone = Europe/Paris' > /etc/php.d/centreon.ini

# Prepare environment for autoinstall.
COPY web/autoinstall.php /usr/share/centreon/autoinstall.php

# Install script (web-wizard install).
COPY web/install.sh /tmp/install.sh
RUN chmod +x /tmp/install.sh
RUN /tmp/install.sh

# Main script.
COPY web/run.sh /usr/share/centreon/container.sh
RUN chmod +x /usr/share/centreon/container.sh
ENTRYPOINT /usr/share/centreon/container.sh
