#!/bin/sh

set -e
set -x

# Start services.
service mysql start

# Give DB access to centreon_map user.
mysql -e "CREATE USER 'centreon_map'@'%' IDENTIFIED BY 'centreon_map'"
mysql -e "GRANT SELECT ON centreon_storage.* TO 'centreon_map'@'%' IDENTIFIED BY 'centreon_map'"
mysql -e "GRANT SELECT ON centreon.* TO 'centreon_map'@'%' IDENTIFIED BY 'centreon_map'"
mysql -e "FLUSH PRIVILEGES"

# Allow admin user to access Centreon API.
mysql -e "UPDATE contact SET reach_api=1 WHERE contact_alias='admin'" centreon

# Create Centreon Broker output (extracted from configure.sh).
CONFIG_GROUP_ID=`mysql -e "SELECT MAX(config_group_id)+1 as config_group_id FROM cfg_centreonbroker_info" centreon | tail -1`
CONFIG_ID=`mysql -e "SELECT min(config_id) FROM cfg_centreonbroker WHERE config_filename LIKE 'central-broker.xml'" centreon | tail -1`
mysql -e "INSERT INTO cfg_centreonbroker_info (config_id, config_key, config_value, config_group, config_group_id, grp_level, subgrp_id, parent_grp_id) VALUES \
          ($CONFIG_ID, 'name', 'Centreon-Studio', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'port', '5758', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'host', '', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'failover', '', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'retry_interval', '', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'buffering_timeout', '0', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'protocol', 'bbdo', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'tls', 'auto', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'private_key', '', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'public_cert', '', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'ca_certificate', '', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'negociation', 'yes', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'one_peer_retention_mode', 'no', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'filters', '', 'output', $CONFIG_GROUP_ID, 0, 1, NULL), \
          ($CONFIG_ID, 'category', 'bam', 'output', $CONFIG_GROUP_ID, 1, NULL, 1), \
          ($CONFIG_ID, 'category', 'neb', 'output', $CONFIG_GROUP_ID, 1, NULL, 1), \
          ($CONFIG_ID, 'compression', 'auto', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'compression_level', '', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'compression_buffer', '', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'type', 'ipv4', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL), \
          ($CONFIG_ID, 'blockId', '1_3', 'output', $CONFIG_GROUP_ID, 0, NULL, NULL)" centreon
centreon -d -u admin -p centreon -a POLLERGENERATE -v 1
centreon -d -u admin -p centreon -a CFGMOVE -v 1

# Install Centreon Map web client.
/tmp/install-centreon-module.php -b /usr/share/centreon/bootstrap.php -m centreon-map4-web-client
mysql -e "UPDATE options SET value='http://map:8080' WHERE \`key\`='map_light_server_address'" centreon

# Stop services.
service mysql stop
