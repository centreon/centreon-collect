# Base information.
FROM ci.int.centreon.com:5000/mon-web:centos6
MAINTAINER Alexandre Fouille <afouille@centreon.com>

# Copy LM sources.
COPY centreon-license-manager /usr/share/centreon/www/modules/centreon-license-manager

# Install script.
COPY install-centreon-module.php /tmp/install-centreon-module.php
COPY lm/lm-install-centos6.sh /tmp/lm-install-centos6.sh
RUN chmod +x /tmp/install-centreon-module.php /tmp/lm-install-centos6.sh
RUN /tmp/lm-install-centos6.sh
