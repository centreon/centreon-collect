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

require_once("../../../../centreon-bam-server.conf.php");
if (file_exists($centreon_path."www/class/Oreon.class.php")) {
    require_once $centreon_path."www/class/Oreon.class.php";
}
require_once("../../../common/header.php");
require_once("../../../common/functions.php");


if (!isset($_GET['sid']) || !isset($_GET['kpi_type']))
	exit();

$kpi_type = $_GET['kpi_type'];

$pearDB = new CentreonBam_Db();
$buffer = new CentreonBam_Xml();

$buffer->startElement("root");

if(isset($_GET['selected'])) {
	$buffer->writeElement("selected", $_GET['selected']);
}
else {
	$buffer->writeElement("selected", 0);
}

if ($kpi_type == "0") {
	$buffer->writeElement("onChange", "yes");
	$buffer->writeElement("onChangeValue", "loadServiceList(this.value)");
	$buffer->writeElement("kpi_type", _("Host"));
	$query = "SELECT host_id, host_name FROM `host` WHERE host_register = '1' ORDER BY host_name";
	$DBRES = $pearDB->query($query);
	$buffer->startElement("kpi");
		$buffer->writeElement("id", 0);
		$buffer->writeElement("name", _("-- Select a Host --"));
	$buffer->endElement();
	while ($row = $DBRES->fetchRow()) {
		$buffer->startElement("kpi");
			$buffer->writeElement("id", $row['host_id']);
			$str = $row["host_name"];
			$str = str_replace("#S#", "/", $str);
			$str = str_replace("#BS#", "\\", $str);
			$buffer->writeElement("name", $str);
		$buffer->endElement();
	}
} elseif ($kpi_type == "1") {
	$buffer->writeElement("onChange", "no");
	$query = "SELECT meta_id, meta_name FROM `meta_service` ORDER BY meta_name";
	$DBRES = $pearDB->query($query);
	$buffer->startElement("kpi");
		$buffer->writeElement("id", 0);
		$buffer->writeElement("name", _("-- Select a Meta Service --"));
	$buffer->endElement();
	while ($row = $DBRES->fetchRow()) {
		$buffer->startElement("kpi");
			$buffer->writeElement("id", $row['meta_id']);
			$buffer->writeElement("name", $row['meta_name']);
		$buffer->endElement();
	}
} elseif ($kpi_type == "2") {
	$buffer->writeElement("onChange", "no");
	$query = "SELECT ba_id, name FROM `mod_bam` ORDER BY name";
	$DBRES = $pearDB->query($query);
	$buffer->startElement("kpi");
		$buffer->writeElement("id", 0);
		$buffer->writeElement("name", _("-- Select a Business Activity --"));
	$buffer->endElement();
	while ($row = $DBRES->fetchRow()) {
		$buffer->startElement("kpi");
			$buffer->writeElement("id", $row['ba_id']);
			$buffer->writeElement("name", $row['name']);
		$buffer->endElement();
	}
} elseif ($kpi_type == "3") {
	$buffer->writeElement("onChange", "no");
	$query = "SELECT boolean_id, name FROM `mod_bam_boolean` ORDER BY name";
	$res = $pearDB->query($query);
	$buffer->startElement("kpi");
	$buffer->writeElement("id", 0);
	$buffer->writeElement("name", _("-- Select a Boolean rule --"));
	$buffer->endElement();
	while ($row = $res->fetchRow()) {
		$buffer->startElement("kpi");
		$buffer->writeElement("id", $row['boolean_id']);
		$buffer->writeElement("name", $row['name']);
		$buffer->endElement();
	}
}

$buffer->endElement();

header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');
$buffer->output();
?>
