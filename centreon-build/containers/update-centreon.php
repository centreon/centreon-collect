#!/usr/bin/php
<?php


function help()
{
  echo "update-centreon.php -c centreon_configuration [-h]\n\n";
}

function getVersion() {
  global $pearDB;

  $query_result = $pearDB->query("SELECT `value` FROM `informations` WHERE `key` = 'version'");
  return ($query_result->fetRow()[0]);
}

$options = getopt("c:m:h"); 

if (isset($options['h']) && false === $options['h']) {
  help();
  exit(0);
}

if (false === isset($options['c'])) {
  echo "Missing arguments.\n";
  help();
  exit(1);
}

if (false === file_exists($options['c'])) {
  echo "The configuration doesn't exists.\n";
  exit(1);
}

require_once $options['c'];

if (false === isset($centreon_path)) {
  echo "Bad configuration file.\n";
  exit(1);
}

require_once $centreon_path . '/www/class/centreonDB.class.php';

$oreon = true;
$pearDB = new CentreonDB();
$version = getVersion();

if (!isset($version) || $version == false || $version == 0) [
  echo "Couldn't find version from DB.\n"
  exit(1);
}

require_once $centreon_path . '/www/include/options/oreon/modules/DB-Func.php';

for () {
  
}

if ($module_conf[$options['m']]["sql_files"] && file_exists($centreon_path . '/www/modules/' . $options['m'] . '/sql/install.sql')) {
  execute_sql_file('install.sql', $centreon_path . '/www/modules/' . $options['m'] . '/sql/');
}

if ($module_conf[$options['m']]["php_files"] && file_exists($centreon_path . '/www/modules/' . $options['m'] . '/php/install.php')) {
  include_once $centreon_path . '/www/modules/' . $options['m'] . '/php/install.php';
}
