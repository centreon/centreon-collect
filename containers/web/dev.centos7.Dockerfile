FROM ci.int.centreon.com:5000/mon-web-stable:centos7
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Update to the latest Engine and Broker.
COPY centreon/centos7/*.rpm /tmp/
RUN yum -y --nogpgcheck '--disablerepo=*' update /tmp/*.rpm
COPY web/centengine.sh /etc/init.d/centengine
COPY web/cbd.sh /etc/init.d/cbd
RUN chmod +x /etc/init.d/centengine /etc/init.d/cbd

# Centreon update is made through custom scripts.
COPY web/kb.sql /tmp/kb.sql
COPY web/update-centreon.php /tmp/update-centreon.php
COPY web/dev.sh /tmp/install.sh
COPY web/kb.sh /tmp/kb.sh
COPY web/standard.sh /tmp/standard.sh
COPY web/standard.sql /tmp/standard.sql
RUN chmod +x /tmp/install.sh /tmp/kb.sh /tmp/standard.sh
COPY centreon/www/install /usr/share/centreon/www/install
RUN /tmp/install.sh
RUN /tmp/kb.sh
RUN /tmp/standard.sh

# Static sources.
COPY centreon/tmpl/install/centreon.cron /etc/cron.d/centreon
COPY centreon/tmpl/install/centstorage.cron /etc/cron.d/centstorage
COPY centreon/cron /usr/share/centreon/cron
RUN chmod a+x /usr/share/centreon/cron/*
RUN sed -i 's#@CENTREON_ETC@#/etc/centreon#g' /usr/share/centreon/cron/*
COPY centreon/www/sounds /usr/share/centreon/www/sounds
COPY centreon/www/img /usr/share/centreon/www/img
COPY centreon/www/include /usr/share/centreon/www/include
COPY centreon/www/modules /usr/share/centreon/www/modules
COPY centreon/www/index.php /usr/share/centreon/www/index.php
COPY centreon/www/main.php /usr/share/centreon/www/main.php
COPY centreon/www/Themes /usr/share/centreon/www/Themes
COPY centreon/www/widgets /usr/share/centreon/www/widgets
COPY centreon/www/api /usr/share/centreon/www/api
COPY centreon/www/class /usr/share/centreon/www/class
COPY centreon/www/lib /usr/share/centreon/www/lib
COPY centreon/config/wiki.conf.php /usr/share/centreon/config/wiki.conf.php
COPY centreon/cron/centKnowledgeSynchronizer.php /usr/share/centreon/cron/centKnowledgeSynchronizer.php
