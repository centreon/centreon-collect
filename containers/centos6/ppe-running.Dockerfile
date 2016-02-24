# Base information.
FROM monitoring-running:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Copy PPE sources.
COPY centreon-export /usr/share/centreon/www/modules/

# Install script.
COPY ppe-install.sh /tmp/ppe-install.sh
RUN chmod +x /tmp/ppe-install.sh
RUN /tmp/ppe-install.sh
