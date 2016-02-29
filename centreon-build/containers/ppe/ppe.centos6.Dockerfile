# Base information.
FROM ci.int.centreon.com:5000/mon-web:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Copy PPE sources.
COPY centreon-export /usr/share/centreon/www/modules/

# Install script.
COPY ppe/ppe-install.sh /tmp/ppe-install.sh
RUN chmod +x /tmp/ppe-install.sh
RUN /tmp/ppe-install.sh
