<?php

session_start();

$_SESSION['step'] = 6;
$_SESSION['DB_USER'] = 'centreon';
$_SESSION['DB_PASS'] = 'centreon';
$_SESSION['ADDRESS'] = 'localhost';
$_SESSION['DB_PORT'] = '3306';
$_SESSION['CONFIGURATION_DB'] = 'centreon';
$_SESSION['STORAGE_DB'] = 'centreon_storage';
$_SESSION['UTILS_DB'] = 'centreon';
$_SESSION['CENTREON_ETC'] = '/etc/centreon/';
$_SESSION['INSTALL_DIR_CENTREON'] = '/usr/share/centreon/';
$_SESSION['CENTREON_ENGINE_CONNECTORS'] = '/usr/lib64/connector/';
$_SESSION['CENTREON_ENGINE_LIB'] = '/usr/lib64/centreon-engine/';
$_SESSION['CENTREON_LOG'] = '/var/log/centreon/';
$_SESSION['CENTREON_RRD_DIR'] = '/var/lib/centreon/';
$_SESSION['CENTREON_VARLIB'] = '/var/lib/centreon/';
$_SESSION['MONITORING_ENGINE'] = 'centreon-engine';
$_SESSION['MONITORING_BINARY'] = '/usr/sbin/centengine';
$_SESSION['MONITORINGENGINE_USER'] = 'centreon-engine';
$_SESSION['MONITORINGENGINE_GROUP'] = 'centreon-engine';
$_SESSION['MONITORINGENGINE_ETC'] = '/etc/centreon-engine/';
$_SESSION['MONITORING_INIT_SCRIPT'] = '/etc/init.d/centengine';
$_SESSION['MONITORINGENGINE_PLUGIN'] = '/usr/lib64/centreon-engine/';
$_SESSION['MONITORING_VAR_LOG'] = '/var/log/centreon-engine/';
$_SESSION['MONITORING_VAR_LIB'] = '/var/lib/centreon-engine/';
$_SESSION['BROKER_MODULE'] = 'centreon-broker';
$_SESSION['BROKER_INIT_SCRIPT'] = '/etc/init.d/cbd';
$_SESSION['CENTREONBROKER_CBMOD'] = '/usr/lib64/nagios/cbmod.so';
$_SESSION['BIN_RRDTOOL'] = '/usr/bin/rrdtool';
$_SESSION['root_password'] = '';
$_SESSION['BIN_MAIL'] = '/bin/mail';
$_SESSION['CURRENT_VERSION'] = '2.7.1';
$_SESSION['version'] = '2.7.1';
$_SESSION['firstname'] = 'admin';
$_SESSION['lastname'] = 'admin';
$_SESSION['ADMIN_PASSWORD'] = 'centreon';
$_SESSION['email'] = 'nonexistent@centreon.com';

?>
