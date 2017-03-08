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

require_once "../centreon-bam-server.conf.php";

require_once $centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Xml.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Db.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Host.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Service.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Media.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Meta.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Utils.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/Ba.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/Kpi.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/Boolean.php";

$clean_str = file_get_contents('php://input');
if (isset($_POST['raw'])) {
    $clean_str = str_replace('\\', '', $_POST['raw']);
}
$xmlString = simplexml_load_string($clean_str);
$buffer = new CentreonBam_Xml();
$pearDB = new CentreonBam_Db();
$utils = new CentreonBam_Utils($pearDB);
$pearDBndo = new CentreonBam_Db("centstorage");
$pearDBO = new CentreonBam_Db("centstorage");

$hObj = new CentreonBam_Host($pearDB);
$sObj = new CentreonBam_Service($pearDB);
$metaObj = new CentreonBam_Meta($pearDB);
$baObj = new CentreonBam_Ba($pearDB, null, $pearDBndo);
$kpiObj = new CentreonBam_Kpi($pearDB, $hObj, $sObj, $metaObj, $baObj);

$mediaObj = new CentreonBam_Media($pearDB);
$boolObj = new CentreonBam_Boolean($pearDB, $pearDBO);

$buffer->startElement("data");
$authorization = false;
if ($xmlString === false) {
	$buffer->startElement("error");
	$buffer->writeAttribute("code", "20000");
	$buffer->text("XML is not valid");
	$buffer->endElement();
} else {
	if (!isset($xmlString->header->user['id']) || !isset($xmlString->header->user['session']) || !isset($xmlString->header->module['name'])){
		$buffer->startElement("error");
		$buffer->writeAttribute("code", "20001");
		$buffer->text("Data is not valid");
		$buffer->endElement();
	} else {
		$uid = $xmlString->header->user['id'];
		$sid = $xmlString->header->user['session'];
		$module_name = $xmlString->header->module['name'];
		$DBRES = $pearDB->query("SELECT * FROM `session` WHERE user_id = '".$uid."' AND session_id = '".$sid."'");
		if (!$DBRES->numRows()) {
			$buffer->startElement("error");
			$buffer->writeAttribute("code", "20002");
			$buffer->text("Invalid session");
			$buffer->endElement();
		} else {
			$module_path = $centreon_path . "www/modules/centreon-bam-server/api_modules/" . $module_name . "/index.php";
			if (!file_exists($module_path)) {
				$buffer->startElement("error");
				$buffer->writeAttribute("code", "20003");
				$buffer->text("No action for this module");
				$buffer->endElement();
			} else {
				$authorization = true;
				require_once ($module_path);
			}
		}
	}
}
$buffer->endElement();

header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');
$buffer->output();
?>
