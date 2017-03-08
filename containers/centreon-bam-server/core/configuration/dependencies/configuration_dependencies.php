<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2016 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */


require_once ("./modules/centreon-bam-server/core/common/header.php");
require_once ("./modules/centreon-bam-server/centreon-bam-server.conf.php");
require_once ("./modules/centreon-bam-server/core/common/functions.php");

#Pear library
require_once "HTML/QuickForm.php";
require_once 'HTML/QuickForm/advmultiselect.php';
require_once 'HTML/QuickForm/Renderer/ArraySmarty.php';

# Smarty template Init
try {
    $centreonDb = new CentreonBam_Db();
    $centreonMonitoringDb = new CentreonBam_Db("centstorage");
    $baObj = new CentreonBam_Ba($centreonDb, null, $centreonMonitoringDb);

    $dependencyObj = new CentreonBam_Dependency($centreonDb, $baObj);

    if (isset($_POST["o1"]) && isset($_POST["o2"])){
        if ($_POST["o1"] != "")
            $o = $_POST["o1"];
        if ($_POST["o2"] != "")
            $o = $_POST["o2"];
    }

    isset($_GET['dep_id']) ? $dep_id = $_GET['dep_id'] : $dep_id = NULL;
    isset($_POST['dep_id']) ? $dep_id = $_POST['dep_id'] : $dep_id = $dep_id;

    isset($_GET['select']) ? $select = $_GET['select'] : $select = array();
    isset($_POST['select']) ? $select = $_POST['select'] : $select = $select;

    switch($o) {
        case "a" :
            require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/dependencies/configuration_dependencies_form.php");
            break; #Add a Dependency
        case "c" :
            require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/dependencies/configuration_dependencies_form.php");
            break; #Modify a Dependency
        case "d":
            CentreonBam_Dependency::deleteByIds($centreonDb, array_keys($select));
            require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/dependencies/configuration_dependencies_list.php");
            break;
        default :
            require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/dependencies/configuration_dependencies_list.php");
            break;
    }
} catch (Exception $ex) {
    $logObj = new \CentreonLog();
    $logObj->insertLog(2, $ex->getMessage());
}
