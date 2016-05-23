#!/usr/bin/php
<?php


function help()
{
  echo "update-centreon.php -c centreon_configuration [-h]\n\n";
}

function getVersion() {
  global $pearDB;

  $query_result = $pearDB->query("SELECT `value` FROM `informations` WHERE `key` = 'version'");
  $row = $query_result->fetchRow();
  return ($row['value']);
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

$pearDB = new CentreonDB();
$oreon = true;

require_once $centreon_path . '/www/include/options/oreon/modules/DB-Func.php';

while (true) {
  $version = getVersion();
  if (!isset($version) || $version == false || $version == 0) {
    echo "Couldn't find version from DB.\n";
    exit(1);
  }

  unset($next);
  if ($handle = opendir($centreon_path . '/www/install/sql/centreon')) {
    while (false !== ($file = readdir($handle))) {
      if (preg_match('/Update-DB-'.preg_quote($version).'_to_([a-zA-Z0-9\-\.]+)\.sql/', $file, $matches)) {
        $next = $matches[1];
        break;
      }
    }
    closedir($handle);
  }

  if (isset($next)) {
    $_POST['current'] = $version;
    $_POST['next'] = $next;
    chdir($centreon_path . '/www/install/step_upgrade/process/');
    include $centreon_path . '/www/install/step_upgrade/process/process_step4.php';
  }
  else break;
}

