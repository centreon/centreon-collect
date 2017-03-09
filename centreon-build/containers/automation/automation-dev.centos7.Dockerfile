# Base information.
FROM ci.int.centreon.com:5000/mon-automation:centos7
MAINTAINER Kevin Duret <kduret@centreon.com>

COPY install-centreon-module.php /tmp/install-centreon-module.php

# Database.
COPY automation/install-dev.sh /tmp/install.sh
COPY centreon-automation/sql /usr/share/centreon/www/modules/centreon-automation/sql
RUN /tmp/install.sh

# Sources.
COPY centreon-automation/tools /usr/share/centreon/www/modules/centreon-automation/tools
COPY centreon-automation/php /usr/share/centreon/www/modules/centreon-automation/php
COPY centreon-automation/centreon-automation.conf.php /usr/share/centreon/www/modules/centreon-automation/centreon-automation.conf.php
COPY centreon-automation/generate_files /usr/share/centreon/www/modules/centreon-automation/generate_files
COPY centreon-automation/webServices /usr/share/centreon/www/modules/centreon-automation/webServices
COPY centreon-automation/backend /usr/share/centreon/www/modules/centreon-automation/backend
COPY centreon-automation/frontend /usr/share/centreon/www/modules/centreon-automation/frontend
