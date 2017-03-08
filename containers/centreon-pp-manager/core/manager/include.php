<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2015 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

define('PAGE_ID', 65001);

/**
 * require configuration files
 */
require_once "/etc/centreon/centreon.conf.php";

/**
 * Require Centreon Classes
 */
require_once $centreon_path . "/www/modules/centreon-pp-manager/core/class/centreonPluginPack.class.php";
require_once $centreon_path . "/www/modules/centreon-pp-manager/core/class/centreonDBManager.class.php";

require_once $centreon_path . '/www/class/centreon.class.php';
require_once $centreon_path . '/www/class/centreonUser.class.php';
require_once $centreon_path . '/www/class/centreonACL.class.php';
require_once $centreon_path . '/www/class/centreonDB.class.php';

session_start();

$centreon = $_SESSION['centreon'];
