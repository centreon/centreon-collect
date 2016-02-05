# Base information.
FROM monitoring-dependencies:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Install Centreon Web.
RUN yum install -y centreon-base-config-centreon-engine mysql-server git
RUN echo 'date.timezone = Europe/Paris' > /etc/php.d/centreon.ini

# Prepare environment for autoinstall.
RUN mkdir /usr/share/centreon/autoinstall
COPY centreon_autoinstall.php /usr/share/centreon/autoinstall.php

# Main scripts that will be called when container is run.
COPY centreon.sh /usr/bin/centreon
RUN chmod +x /usr/bin/centreon
ENTRYPOINT /usr/bin/centreon
