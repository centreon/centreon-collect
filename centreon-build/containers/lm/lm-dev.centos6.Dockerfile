# Base information.
FROM ci.int.centreon.com:5000/mon-lm:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Database.
COPY lm/install-dev.sh /tmp/install.sh
COPY centreon-license-manager/sql /usr/share/centreon/www/modules/centreon-license-manager/sql
RUN /tmp/install.sh

# Sources.
COPY centreon-license-manager/centreon-license-manager.conf.php /usr/share/centreon/www/modules/centreon-license-manager/centreon-license-manager.conf.php
COPY centreon-license-manager/webServices /usr/share/centreon/www/modules/centreon-license-manager/webServices
COPY centreon-license-manager/backend /usr/share/centreon/www/modules/centreon-license-manager/backend
COPY centreon-license-manager/frontend /usr/share/centreon/www/modules/centreon-license-manager/frontend
