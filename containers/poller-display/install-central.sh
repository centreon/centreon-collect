#!/bin/sh

set -e
set -x

# Get Broker I/O ID from configuration name and I/O object name.
function get_input_id {
  centreon -u admin -p centreon -o CENTBROKERCFG -a LISTINPUT -v "$1" | grep "$2" | cut -d ';' -f 1
}
function get_output_id {
  centreon -u admin -p centreon -o CENTBROKERCFG -a LISTOUTPUT -v "$1" | grep "$2" | cut -d ';' -f 1
}

# Start services.
service mysql start
httpd -k start

# Install Centreon Web module.
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-poller-display-central

# Create and populate poller (that will have Poller Display).
centreon -u admin -p centreon -o INSTANCE -a ADD -v 'Poller;poller;22;'
mysql centreon -e "INSERT INTO mod_poller_display_server_relations (nagios_server_id) SELECT id FROM nagios_server WHERE name='Poller'"
centreon -u admin -p centreon -o HOST -a ADD -v 'Poller-Server;Poller-Server;poller;generic-host;Poller;'
centreon -u admin -p centreon -o SERVICE -a ADD -v 'Poller-Server;Ping;Ping-LAN'

# centengine.cfg
centreon -u admin -p centreon -o ENGINECFG -a ADD -v 'Centreon Engine Poller;Poller;Centreon Engine'
centreon -u admin -p centreon -o ENGINECFG -a SETPARAM -v 'Centreon Engine Poller;broker_module;/usr/lib64/centreon-engine/externalcmd.so|/usr/lib64/nagios/cbmod.so /etc/centreon-broker/poller-module.xml'

# Poller's cbmod.
centreon -u admin -p centreon -o CENTBROKERCFG -a ADD -v 'poller-module;Poller'
centreon -u admin -p centreon -o CENTBROKERCFG -a SETPARAM -v 'poller-module;filename;poller-module.xml'
centreon -u admin -p centreon -o CENTBROKERCFG -a SETPARAM -v 'poller-module;daemon;0'
centreon -u admin -p centreon -o CENTBROKERCFG -a SETPARAM -v 'poller-module;cache_directory;/var/lib/centreon-broker/'
centreon -u admin -p centreon -o CENTBROKERCFG -a ADDOUTPUT -v 'poller-module;output-central;ipv4'
IOID=`get_output_id poller-module output-central`
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-module;$IOID;host;web"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-module;$IOID;port;5669"
centreon -u admin -p centreon -o CENTBROKERCFG -a ADDOUTPUT -v 'poller-module;output-local;ipv4'
IOID=`get_output_id poller-module output-local`
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-module;$IOID;host;localhost"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-module;$IOID;port;5669"

# Poller's local SQL database.
centreon -u admin -p centreon -o CENTBROKERCFG -a ADD -v 'poller-sql;Poller'
centreon -u admin -p centreon -o CENTBROKERCFG -a SETPARAM -v 'poller-sql;filename;poller-sql.xml'
centreon -u admin -p centreon -o CENTBROKERCFG -a SETPARAM -v 'poller-sql;daemon;1'
centreon -u admin -p centreon -o CENTBROKERCFG -a SETPARAM -v 'poller-sql;cache_directory;/var/lib/centreon-broker/'
centreon -u admin -p centreon -o CENTBROKERCFG -a ADDINPUT -v 'poller-sql;input;ipv4'
IOID=`get_input_id poller-sql input`
centreon -u admin -p centreon -o CENTBROKERCFG -a SETINPUT -v "poller-sql;$IOID;port;5669"
centreon -u admin -p centreon -o CENTBROKERCFG -a ADDOUTPUT -v 'poller-sql;output-sql;sql'
IOID=`get_output_id poller-sql output-sql`
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_type;mysql"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_name;centreon_storage"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_host;localhost"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_port;3306"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_user;root"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_password;"
centreon -u admin -p centreon -o CENTBROKERCFG -a ADDOUTPUT -v 'poller-sql;output-storage;storage'
IOID=`get_output_id poller-sql output-storage`
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_type;mysql"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_name;centreon_storage"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_host;localhost"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_port;3306"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_user;root"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;db_password;"
centreon -u admin -p centreon -o CENTBROKERCFG -a ADDOUTPUT -v 'poller-sql;output-rrd;ipv4'
IOID=`get_output_id poller-sql output-rrd`
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;host;localhost"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-sql;$IOID;port;5670"

# Poller's local RRD writer.
centreon -u admin -p centreon -o CENTBROKERCFG -a ADD -v 'poller-rrd;Poller'
centreon -u admin -p centreon -o CENTBROKERCFG -a SETPARAM -v 'poller-rrd;daemon;1'
centreon -u admin -p centreon -o CENTBROKERCFG -a SETPARAM -v 'poller-rrd;cache_directory;/var/lib/centreon-broker/'
centreon -u admin -p centreon -o CENTBROKERCFG -a SETPARAM -v 'poller-rrd;filename;poller-rrd.xml'
centreon -u admin -p centreon -o CENTBROKERCFG -a ADDINPUT -v 'poller-rrd;input;ipv4'
IOID=`get_input_id poller-rrd input`
centreon -u admin -p centreon -o CENTBROKERCFG -a SETINPUT -v "poller-rrd;$IOID;port;5670"
centreon -u admin -p centreon -o CENTBROKERCFG -a ADDOUTPUT -v 'poller-rrd;output-rrd;rrd'
IOID=`get_output_id poller-rrd output-rrd`
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-rrd;$IOID;metrics_path;/var/lib/centreon/metrics"
centreon -u admin -p centreon -o CENTBROKERCFG -a SETOUTPUT -v "poller-rrd;$IOID;status_path;/var/lib/centreon/status"

# Stop services.
httpd -k stop
service mysql stop
