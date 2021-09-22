#!/bin/sh

/bin/cat /usr/share/centreon/www/install/install.conf.php | while read line; do
	varname=`echo $line | /bin/sed -e 's/^\$conf_centreon\['\''\([a-z_][a-z_]*\)'\''\].*=.*$/\1/g' | tr '[:lower:]' '[:upper:]'`
	value=""
	value=`echo $line | /bin/awk -F= '{ print $2 }' | /bin/sed -e 's/^[ ]*//g' -e 's/"//g' -e 's/[ ]*;$//g'`
	if [ -n "$value" ]; then
        	case $varname in
			MAIL)
				echo "s:@MAILER@:$value:g" >> /usr/share/centreon/install/sql_macros.sed
				;;
			BROKER_ETC)
				echo "s:@CENTREONBROKER_ETC@:$value:g" >> /usr/share/centreon/install/sql_macros.sed
				;;
 			MONITORING_VARLOG)
				echo "s:@MONITORING_VAR_LOG@:$value:g" >> /usr/share/centreon/install/sql_macros.sed
				;;
			MONITORING_USER)
				echo "s:@MONITORINGENGINE_USER@:$value:g" >> /usr/share/centreon/install/sql_macros.sed
				;;
			MONITORING_GROUP)
				echo "s:@MONITORINGENGINE_GROUP@:$value:g" >> /usr/share/centreon/install/sql_macros.sed
				;;
			MONITORING_ETC)
				echo "s:@MONITORINGENGINE_ETC@:$value:g" >> /usr/share/centreon/install/sql_macros.sed
				;;
			PLUGIN_DIR)
				echo "s:@PLUGIN_DIR@:$value:g" >> /usr/share/centreon/install/sql_macros.sed
				echo "s:@NAGIOS_PLUGIN@:$value:g" >> /usr/share/centreon/install/sql_macros.sed
				;;
			*)
				echo "s:@$varname@:$value:g" >> /usr/share/centreon/install/sql_macros.sed
				;;
		esac
	fi
done

## Add other values
echo "s:@CENTREONBROKER_LOG@:/var/log/centreon-broker:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@CENTREONBROKER_VARLIB@:/var/lib/centreon-broker:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@CENTREONBROKER_CBMOD@:/usr/lib/nagios/cbmod.so:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@MONITORINGENGINE_PLUGIN@:/usr/lib/nagios/plugins:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@MONITORING_VAR_LIB@:/var/lib/centreon-engine:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@DB_HOST@:localhost:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@DB_PORT@:3306:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@DB_USER@:centreon:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@UTILS_DB@:centreon_status:g" >> /usr/share/centreon/install/sql_macros.sed
if [ `uname -p` = "x86_64" ]; then
    echo "s:@NDOMOD_BINARY@:/usr/lib64/nagios/ndomod.o:g" >> /usr/share/centreon/install/sql_macros.sed
else 
    echo "s:@NDOMOD_BINARY@:/usr/lib/nagios/ndomod.o:g" >> /usr/share/centreon/install/sql_macros.sed
fi
echo "s:@CENTREON_ENGINE_STATS_BINARY@:/usr/sbin/centenginestats:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@NAGIOS_BINARY@:/usr/sbin/nagios:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@NAGIOSTATS_BINARY@:/usr/bin/nagiostats:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@NAGIOS_INIT_SCRIPT@:/etc/init.d/nagios:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@ADMIN_PASSWORD@:centreon:g" >> /usr/share/centreon/install/sql_macros.sed
echo "s:@STORAGE_DB@:centreon_storage:g" >> /usr/share/centreon/install/sql_macros.sed
if [ `uname -p` = "x86_64" ]; then
	echo "s:@CENTREON_ENGINE_LIB@:/usr/lib64/centreon-engine:g" >> /usr/share/centreon/install/sql_macros.sed
	echo "s:@CENTREONBROKER_LIB@:/usr/share/centreon/lib/centreon-broker:g" >> /usr/share/centreon/install/sql_macros.sed
else
	echo "s:@CENTREON_ENGINE_LIB@:/usr/lib/centreon-engine:g" >> /usr/share/centreon/install/sql_macros.sed
	echo "s:@CENTREONBROKER_LIB@:/usr/share/centreon/lib/centreon-broker:g" >> /usr/share/centreon/install/sql_macros.sed
fi
