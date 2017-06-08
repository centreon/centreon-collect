#!/usr/bin/php
<?php

class DummyCentreonUser
{
    public $user_id;
    public function __construct()
    {
        $this->user_id = 1;
    }
}

class DummyCentreon
{
    public $user;
    public function __construct()
    {
        $this->user = new DummyCentreonUser();
    }
}

function help()
{
  echo "install-centreon-widget.php -c centreon_configuration -w widget_name [-h]\n\n";
}

$options = getopt("c:w:hu");

if (isset($options['h']) && false === $options['h']) {
  help();
  exit(0);
}

if (false === isset($options['c']) || false === isset($options['w'])) {
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

if (false == is_dir($centreon_path . '/www/widgets/' . $options['w'])) {
  echo "The widget directory is not installed.\n";
  exit(1);
}

require_once $centreon_path . '/www/class/centreonDB.class.php';

$oreon = true;
$pearDB = new CentreonDB();

require_once $centreon_path . '/www/include/options/oreon/modules/DB-Func.php';
require_once $centreon_path . '/www/class/centreonWidget.class.php';

$widgetObj = new CentreonWidget(new DummyCentreon, $pearDB);
$widgetObj->install($centreon_path . '/www/widgets', $options['w']);
