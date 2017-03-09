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
require_once "HTML/QuickForm.php";
require_once 'HTML/QuickForm/advmultiselect.php';
require_once 'HTML/QuickForm/Renderer/ArraySmarty.php';

// Smarty template Init
$pearDB = new CentreonBam_Db();
$boolObj = new CentreonBam_Boolean($pearDB);

$hst = new CentreonBam_Host($pearDB);
$svc = new CentreonBam_Service($pearDB);

if (isset($_POST["o1"]) && isset($_POST["o2"])){
	if ($_POST["o1"] != "")
		$o = $_POST["o1"];
	if ($_POST["o2"] != "")
		$o = $_POST["o2"];
}

$boolean_id = isset($_GET['boolean_id']) ? $_GET['boolean_id'] : NULL;
$boolean_id = isset($_POST['boolean_id']) ? $_POST['boolean_id'] : $boolean_id;

isset($_GET['select']) ? $select = $_GET['select'] : $select = NULL;
isset($_POST['select']) ? $select = $_POST['select'] : $select = $select;

switch($o) {
	case "a" : require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/boolean/configuration_boolean_form.php");
				break;
	
	case "c" : require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/boolean/configuration_boolean_form.php"); 
				break;
	
	case "d" : $boolObj->delete($boolean_id); 
				require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/boolean/configuration_boolean_list.php");
				break;
	
	case "md" : $boolObj->delete($select); 
				require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/boolean/configuration_boolean_list.php");
				break;
	
	case "dp" : $boolObj->duplicate($select);	
				require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/boolean/configuration_boolean_list.php");
				break;
	case "ma" : 
                    $boolObj->setStatus($select, 1);
                    require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/boolean/configuration_boolean_list.php");
		    break;
        case "mu" : 
                    $boolObj->setStatus($select, 0);
                    require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/boolean/configuration_boolean_list.php");
		    break;          
	default :  require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/boolean/configuration_boolean_list.php"); break;
}
