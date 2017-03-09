# Base information.
FROM ci.int.centreon.com:5000/mon-ppe:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Database.
COPY ppe/install-dev.sh /tmp/install.sh
COPY centreon-export/sql /usr/share/centreon/www/modules/centreon-export/sql
RUN /tmp/install.sh

# Static sources.
COPY centreon-export/webServices /usr/share/centreon/www/modules/centreon-export/webServices
COPY centreon-export/core /usr/share/centreon/www/modules/centreon-export/core
