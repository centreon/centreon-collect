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
if (file_exists($centreon_path . "www/class/Oreon.class.php")) {
    require_once($centreon_path . "www/class/Oreon.class.php");
}
require_once("../../../common/header.php");
require_once("../../../common/functions.php");

if (!isset($_GET['sid']) || !isset($_GET['obj_type']) || !isset($_GET['selected'])) {
    exit();
}

$obj_type = $_GET['obj_type'];
$selected = $_GET['selected'];

$pearDB = new CentreonBam_Db();
$buffer = new CentreonBam_Xml();
$options = new CentreonBam_Options($pearDB, 0);
$kpiObj = new CentreonBam_Kpi($pearDB);
$opt = $options->getGeneralOptions();
$buffer->startElement("root");

$kpi_count = 0;

if ($obj_type == 0) {

    $query = "SELECT host_id, service_id, host_name, service_description 
                   FROM `host_service_relation` hsr, `service` s, `host` h
                   WHERE hsr.host_host_id = " . $pearDB->escape($selected) . "
                   AND hsr.host_host_id = h.host_id 
                   AND hsr.service_service_id = s.service_id 
                   UNION
                   SELECT host_id, service_id, host_name, service_description
                   FROM `host_service_relation` hsr, `service` s, `hostgroup_relation` hgr, `host` h
                   WHERE hsr.hostgroup_hg_id = hgr.hostgroup_hg_id
                   AND hgr.host_host_id = h.host_id
                   AND hsr.service_service_id = s.service_id
                   AND h.host_id = " . $pearDB->escape($selected) . "
                   ORDER BY service_description";

    $DBRES = $pearDB->query($query);
    $style = "list_two";
    while ($row = $DBRES->fetchRow()) {
        $buffer->startElement("kpi");
        $buffer->writeElement("host_id", $row['host_id']);
        $buffer->writeElement("service_id", $row['service_id']);
        $host_name = str_replace("#S#", "/", $row['host_name']);
        $host_name = str_replace("#BS#", "\\", $host_name);
        $buffer->writeElement("host_name", $host_name);
        $svc_desc = str_replace("#S#", "/", $row['service_description']);
        $svc_desc = str_replace("#BS#", "\\", $svc_desc);
        $buffer->writeElement("service_desc", $svc_desc);
        if ($style == "list_two") {
            $style = "list_one";
        } else {
            $style = "list_two";
        }
        $buffer->writeElement("style", $style);
        $buffer->endElement();
        $kpi_count++;
    }
} elseif ($obj_type == 1) {

    $query = "SELECT hgr.host_host_id, h.host_name " .
            "FROM `hostgroup_relation` hgr, host h " .
            "WHERE hgr.hostgroup_hg_id = '" . $pearDB->escape($selected) . "' " .
            "AND hgr.host_host_id = h.host_id " .
            "ORDER BY h.host_name";

    $DBRES = $pearDB->query($query);
    while ($row = $DBRES->fetchRow()) {
        $query2 = "SELECT host_id, service_id, host_name, service_description 
                   FROM `host_service_relation` hsr, `service` s, `host` h
                   WHERE hsr.host_host_id = " . $pearDB->escape($row['host_host_id']) . "
                   AND hsr.host_host_id = h.host_id 
                   AND hsr.service_service_id = s.service_id 
                   UNION
                   SELECT host_id, service_id, host_name, service_description
                   FROM `host_service_relation` hsr, `service` s, `hostgroup_relation` hgr, `host` h
                   WHERE hsr.hostgroup_hg_id = hgr.hostgroup_hg_id
                   AND hgr.host_host_id = h.host_id
                   AND hsr.service_service_id = s.service_id
                   AND h.host_id = " . $pearDB->escape($row['host_host_id']) . "
                   ORDER BY service_description";
        $DBRES2 = $pearDB->query($query2);
        $style = "list_two";
        while ($row2 = $DBRES2->fetchRow()) {
            $buffer->startElement("kpi");
            $buffer->writeElement("host_id", $row2['host_id']);
            $buffer->writeElement("service_id", $row2['service_id']);
            $host_name = str_replace("#S#", "/", $row2['host_name']);
            $host_name = str_replace("#BS#", "\\", $host_name);
            $buffer->writeElement("host_name", $host_name);
            $svc_desc = str_replace("#S#", "/", $row2['service_description']);
            $svc_desc = str_replace("#BS#", "\\", $svc_desc);
            $buffer->writeElement("service_desc", $svc_desc);
            if ($style == "list_two") {
                $style = "list_one";
            } else {
                $style = "list_two";
            }
            $buffer->writeElement("style", $style);
            $buffer->endElement();
            $kpi_count++;
        }
    }
} elseif ($obj_type == 2) {
    $query = "SELECT s.service_id, s.service_description, h.host_id, h.host_name " .
            "FROM `servicegroup_relation` sgr, host h, service s " .
            "WHERE sgr.servicegroup_sg_id = '" . $pearDB->escape($selected) . "' " .
            "AND sgr.host_host_id = h.host_id " .
            "AND sgr.service_service_id = s.service_id " .
            "ORDER BY h.host_name, s.service_description";

    $DBRES = $pearDB->query($query);
    while ($row = $DBRES->fetchRow()) {
        $style = "list_two";
        $buffer->startElement("kpi");
        $buffer->writeElement("host_id", $row['host_id']);
        $buffer->writeElement("service_id", $row['service_id']);
        $host_name = str_replace("#S#", "/", $row['host_name']);
        $host_name = str_replace("#BS#", "\\", $host_name);
        $buffer->writeElement("host_name", $host_name);
        $svc_desc = str_replace("#S#", "/", $row['service_description']);
        $svc_desc = str_replace("#BS#", "\\", $svc_desc);
        $buffer->writeElement("service_desc", $svc_desc);
        if ($style == "list_two") {
            $style = "list_one";
        } else {
            $style = "list_two";
        }
        $buffer->writeElement("style", $style);
        $buffer->endElement();
        $kpi_count++;
    }
}
$buffer->startElement("info");
$buffer->writeElement("obj_type", $obj_type);
$buffer->writeElement("host_label", _("Host"));
$buffer->writeElement("svc_label", _("Service"));
$buffer->writeElement("warning_label", _("Warning Impact"));
$buffer->writeElement("critical_label", _("Critical Impact"));
$buffer->writeElement("unknown_label", _("Unknown Impact"));
$buffer->writeElement("default_warning", $opt['kpi_warning_impact']);
$buffer->writeElement("default_critical", $opt['kpi_critical_impact']);
$buffer->writeElement("default_unknown", $opt['kpi_unknown_impact']);
$buffer->writeElement("button_label", _("Add KPI"));
$buffer->writeElement("no_entry", _("No KPI found"));
$buffer->writeElement("number_of_kpi", $kpi_count);
$buffer->endElement();

$query = "SELECT * FROM mod_bam_impacts ORDER BY code";
$res = $pearDB->query($query);
while ($row = $res->fetchRow()) {
    $buffer->startElement("impact");
    $buffer->writeElement("code", $row['id_impact']);
    $buffer->writeElement("color", $row['color']);
    $buffer->writeElement("label", $kpiObj->getCriticityLabel($row['code']));
    $buffer->endElement();
}

$buffer->endElement();

header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');

$buffer->output();
