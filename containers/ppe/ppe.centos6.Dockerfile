# Base information.
FROM ci.int.centreon.com:5000/mon-web:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Copy PPE sources.
COPY centreon-export /usr/share/centreon/www/modules/

# Install script.
COPY install-centreon-module.php /tmp/install-centreon-module.php
COPY ppe/ppe-install.sh /tmp/ppe-install.sh
RUN chmod +x /tmp/install-centreon-module.php /tmp/ppe-install.sh
RUN /tmp/ppe-install.sh
