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

$pearDB = new CentreonBam_Db();
$baGroup = new CentreonBam_BaGroup($pearDB);

if (isset($_POST["o1"]) && isset($_POST["o2"])){
	if ($_POST["o1"] != "")
		$o = $_POST["o1"];
	if ($_POST["o2"] != "")
		$o = $_POST["o2"];
}

isset($_GET['ba_group_id']) ? $id_ba_group = $_GET['ba_group_id'] : $id_ba_group = NULL;
isset($_POST['ba_group_id']) ? $id_ba_group = $_POST['ba_group_id'] : $id_ba_group = $id_ba_group;

isset($_GET['select']) ? $select = $_GET['select'] : $select = NULL;
isset($_POST['select']) ? $select = $_POST['select'] : $select = $select;

switch($o) {
	case "a" : require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_form.php"); break; #Add a BA group
	
	case "c" : require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_form.php"); break; #Modify a BA group
	
	case "d" : $baGroup->deleteBAGroup($id_ba_group); 
				require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_list.php");
				break; #Deletes a BA group
	
	case "v" : $baGroup->setVisibility($id_ba_group, 1); 
				require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_list.php");
				break; #Sets BA group to visible 
	
	case "i" : $baGroup->setVisibility($id_ba_group, 0); 
				require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_list.php");
				break; #Sets BA group to invisible
	
	case "md" : $baGroup->deleteBAGroup(0, $select); 
				require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_list.php");
				break;
	
	case "mv" : $baGroup->setVisibility(0, 1, $select); 
				require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_list.php");
				break; #Sets BA group to visible
	
	case "mi" : $baGroup->setVisibility(0, 0, $select); 
				require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_list.php");
				break; #Sets BA group to visible
	
	case "dp" : $baGroup->duplicate($select);	
				require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_list.php");
				break; #Duplicate a group of BA
	
	default :  require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_list.php"); break;
}

?>