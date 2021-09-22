#!/bin/sh

set -e
set -x

service mysql start
mysql centreon < /usr/share/centreon/www/modules/centreon-poller-display-central/sql/uninstall.sql
mysql centreon < /usr/share/centreon/www/modules/centreon-poller-display-central/sql/install.sql
mysql centreon -e "INSERT INTO mod_poller_display_server_relations (nagios_server_id) SELECT id FROM nagios_server WHERE name='Poller'"
service mysql stop
