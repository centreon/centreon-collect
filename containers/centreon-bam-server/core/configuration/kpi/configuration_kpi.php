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

// Pear library
require_once 'HTML/QuickForm.php';
require_once 'HTML/QuickForm/advmultiselect.php';
require_once 'HTML/QuickForm/Renderer/ArraySmarty.php';

// Smarty template Init
$pearDB = new CentreonBam_Db();
$pearDBndo = new CentreonBam_Db("centstorage");
$kpi = new CentreonBam_Kpi($pearDB);
$ba = new CentreonBam_Ba($pearDB, null, $pearDBndo);
$hst = new CentreonBam_Host($pearDB);
$svc = new CentreonBam_Service($pearDB);
$boolean = new CentreonBam_Boolean($pearDB);
$meta = new CentreonBam_Meta($pearDB);

if (isset($_POST["o1"]) && isset($_POST["o2"])) {
	if ($_POST["o1"] != "") {
		$o = $_POST["o1"];
    }
	if ($_POST["o2"] != "") {
		$o = $_POST["o2"];
    }
}

isset($_GET['kpi_id']) ? $kpi_id = $_GET['kpi_id'] : $kpi_id = null;
isset($_POST['kpi_id']) ? $kpi_id = $_POST['kpi_id'] : $kpi_id = $kpi_id;

isset($_GET['select']) ? $select = $_GET['select'] : $select = null;
isset($_POST['select']) ? $select = $_POST['select'] : $select = $select;

$boolId = null;
if (isset($_REQUEST['boolId'])) {
    $boolId = $_REQUEST['boolId'];
}

switch($o) {	
	case "a"  : require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_form.php"; 
                    break; 	
	case "c"  : require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_form.php";
                    break; 
	case "ab" : require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_bool_form.php";
                    break; 
	case "cb" : require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_bool_form.php";
                    break; 
	case "am" : require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_multiple_form.php";
                    break;	
	case "sa" : 
        if (!is_null($kpi_id)) {
            $kpi->setActivate($kpi_id, 1);
        } elseif (!is_null($boolId)) {
            $boolean->enable(array($boolId));
        }
        require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_list.php";
	    break;
	case "csv": require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/loadCSV.php";
		    break;
	case "md" : 
        if (!is_null($select)) {
            $kpi->deleteKPI(0, $select); 
        }
        if (!is_null($boolSelect)) {
            $boolean->delete($boolSelect);
        }
	    require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_list.php";
        break;
	case "su" : 
        if (!is_null($kpi_id)) {
            $kpi->setActivate($kpi_id, 0);
        } elseif (!is_null($boolId)) {
            $boolean->disable(array($boolId));
        }
	    require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_list.php";
        break;
	case "ma" : $kpi->setActivate(0, 1, $select);
        require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_list.php";
        break;
	case "mu" : $kpi->setActivate(0, 0, $select);
        require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_list.php";
        break;
	default   : require_once $centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_list.php";
        break;
}

