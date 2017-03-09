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
    $pearDB = new CentreonBam_Db();
    $pearDBndo = new CentreonBam_Db("centstorage");

    $ba = new CentreonBam_Ba($pearDB, null, $pearDBndo);

    if (isset($_POST["o1"]) && isset($_POST["o2"])){
        if ($_POST["o1"] != "") {
            $o = $_POST["o1"];
        }
        if ($_POST["o2"] != "") {
            $o = $_POST["o2"];
        }
    }

    isset($_GET['ba_id']) ? $id_ba = $_GET['ba_id'] : $id_ba = NULL;
    isset($_POST['ba_id']) ? $id_ba = $_POST['ba_id'] : $id_ba = $id_ba;

    isset($_GET['select']) ? $select = $_GET['select'] : $select = NULL;
    isset($_POST['select']) ? $select = $_POST['select'] : $select = $select;

    switch($o) {
        case "a":
            require_once(
                $centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_form.php"
            );
            break; #Add a BA group

        case "c":
            require_once(
                $centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_form.php"
            );
            break; #Modify a BA group

        case "d":
            $ba->deleteBA($id_ba); 
            require_once(
                $centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_list.php"
            );
            break; #Deletes a BA group	

        case "sa":
            $ba->setActivate($id_ba, 1);
            require_once(
                $centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_list.php"
            );
            break;

        case "su":
            $ba->setActivate($id_ba, 0);
            require_once(
                $centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_list.php"
            );
            break;

        case "md":
            $ba->deleteBA(0, $select); 
            require_once(
                $centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_list.php"
            );
            break;

        case "ma":
            $ba->setActivate(0, 1, $select); 
            require_once(
                $centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_list.php"
            );
            break; #Sets BA group to enabled

        case "mu":
            $ba->setActivate(0, 0, $select); 
            require_once(
                $centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_list.php"
            );
            break; #Sets BA group to disabled

        case "dp":
            $ba->duplicate($select); 
            require_once(
                $centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_list.php"
            );
            break; #Duplicates a BA

        default:
            require_once(
                $centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_list.php"
            );
            break;
    }
} catch (Exception $ex) {
    $logObj = new \CentreonLog();
    $logObj->insertLog(2, $ex->getMessage());
}
