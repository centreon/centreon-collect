#!/usr/bin/php
<?php


function help()
{
  echo "install-centreon-module.php -c centreon_configuration -m module_name [-h]\n\n";
}

$options = getopt("c:m:hu");

if (isset($options['h']) && false === $options['h']) {
  help();
  exit(0);
}

if (false === isset($options['c']) || false === isset($options['m'])) {
  echo "Missing arguments.\n";
  exit(1);
}

if (false === file_exists($options['c'])) {
  echo "The configuration doesn't exist.\n";
  exit(1);
}

require_once $options['c'];

if (false === isset($centreon_path)) {
  echo "Bad configuration file.\n";
  exit(1);
}

if (false == is_dir($centreon_path . '/www/modules/' . $options['m'])) {
  echo "The module directory is not installed.\n";
  exit(1);
}

require_once $centreon_path . '/www/class/centreonDB.class.php';

$oreon = true;
$pearDB = new CentreonDB();

require_once $centreon_path . '/www/include/options/oreon/modules/DB-Func.php';
require_once $centreon_path . '/www/modules/' . $options['m'] . '/conf.php';

if (testModuleExistence(null, $options['m'])) {
    if (isset($options['u']) && false === $options['u']) {
        if ($module_conf[$options['m']]["sql_files"] && file_exists($centreon_path . '/www/modules/' . $options['m'] . '/sql/uninstall.sql')) {
            execute_sql_file('uninstall.sql', $centreon_path . '/www/modules/' . $options['m'] . '/sql/');
        }
        if ($module_conf[$options['m']]["php_files"] && file_exists($centreon_path . '/www/modules/' . $options['m'] . '/php/uninstall.php')) {
            include_once $centreon_path . '/www/modules/' . $options['m'] . '/php/uninstall.php';
        }
    } else {
        echo "The module is already installed.\n";
        exit(1);
    }
}

if ($module_conf[$options['m']]["sql_files"] && file_exists($centreon_path . '/www/modules/' . $options['m'] . '/sql/install.sql')) {
  execute_sql_file('install.sql', $centreon_path . '/www/modules/' . $options['m'] . '/sql/');
}

if ($module_conf[$options['m']]["php_files"] && file_exists($centreon_path . '/www/modules/' . $options['m'] . '/php/install.php')) {
  include_once $centreon_path . '/www/modules/' . $options['m'] . '/php/install.php';
}

insertModuleInDB($options['m'], $module_conf[$options['m']]);
