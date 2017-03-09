# Base information.
FROM ci.int.centreon.com:5000/mon-ppm:centos6
MAINTAINER Alexandre Fouille <afouille@centreon.com>

# Database.
COPY ppm/install-dev.sh /tmp/install.sh
RUN chmod +x /tmp/install.sh
COPY centreon-pp-manager/sql /usr/share/centreon/www/modules/centreon-pp-manager/sql
RUN /tmp/install.sh

# Static sources.
COPY centreon-pp-manager/centreon-pluginpack-manager.conf.php /usr/share/centreon/www/modules/centreon-pp-manager/centreon-pluginpack-manager.conf.php
COPY centreon-pp-manager/static /usr/share/centreon/www/modules/centreon-pp-manager/static
COPY centreon-pp-manager/locale /usr/share/centreon/www/modules/centreon-pp-manager/locale
COPY centreon-pp-manager/UPGRADE /usr/share/centreon/www/modules/centreon-pp-manager/UPGRADE
COPY centreon-pp-manager/webServices /usr/share/centreon/www/modules/centreon-pp-manager/webServices
COPY centreon-pp-manager/core /usr/share/centreon/www/modules/centreon-pp-manager/core
