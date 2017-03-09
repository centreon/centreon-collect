# Base information.
FROM ci.int.centreon.com:5000/des-bam:centos7
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

COPY install-centreon-module.php /tmp/install-centreon-module.php

# Database.
COPY bam/dev.sh /tmp/install.sh
RUN chmod +x /tmp/install.sh
COPY centreon-bam-server/sql /usr/share/centreon/www/modules/centreon-bam-server/sql
RUN find /usr/share/centreon/www/modules/centreon-bam-server/sql -type f -exec sed -i -e 's/@DB_CENTSTORAGE@/centreon_storage/g' {} ';'
RUN /tmp/install.sh

# Sources.
COPY centreon-bam-server/locale /usr/share/centreon/www/modules/centreon-bam-server/locale
COPY centreon-bam-server/php /usr/share/centreon/www/modules/centreon-bam-server/php
COPY centreon-bam-server/checklist /usr/share/centreon/www/modules/checklist
COPY centreon-bam-server/UPGRADE /usr/share/centreon/www/modules/centreon-bam-server/UPGRADE
COPY centreon-bam-server/tools /usr/share/centreon/www/modules/centreon-bam-server/tools
COPY centreon-bam-server/engine /usr/share/centreon/www/modules/centreon-bam-server/engine
COPY centreon-bam-server/webServices /usr/share/centreon/www/modules/centreon-bam-server/webServices
COPY centreon-bam-server/restart_pollers /usr/share/centreon/www/modules/centreon-bam-server/restart_pollers
COPY centreon-bam-server/widgets /usr/share/centreon/www/modules/centreon-bam-server/widgets
COPY centreon-bam-server/generate_files /usr/share/centreon/www/modules/centreon-bam-server/generate_files
COPY centreon-bam-server/getGraph /usr/share/centreon/www/modules/centreon-bam-server/getGraph
COPY centreon-bam-server/centreon-clapi /usr/share/centreon/www/modules/centreon-bam-server/centreon-clapi
COPY centreon-bam-server/api_modules /usr/share/centreon/www/modules/centreon-bam-server/api_modules
COPY centreon-bam-server/hooks /usr/share/centreon/www/modules/centreon-bam-server/hooks
COPY centreon-bam-server/core /usr/share/centreon/www/modules/centreon-bam-server/core
