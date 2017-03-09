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
require_once("../../../centreon-bam-server.conf.php");
require_once("../../common/functions.php");

if (file_exists($centreon_path."www/class/centreonDuration.class.php")) {
    require_once $centreon_path."www/class/centreonDuration.class.php";
    $durationObj = new CentreonDuration();
}

require_once("../../common/header.php");

if (!isset($_GET['sid']) || !isset($_GET['bv_id']) || !isset($_GET['p'])) {
	exit();
}

$bv_filter = $_GET['bv_id'];
$session_id = $_GET['sid'];
$p = $_GET['p'];

$pearDB = new CentreonBam_Db();
$pearDBO = new CentreonBam_Db("centstorage");

$ba_acl = new CentreonBam_Acl($pearDB, $oreon->user->user_id);
$tpObj = new CentreonBam_TimePeriod($pearDB);
$buffer = new CentreonBam_Xml();
$bvObj = new CentreonBam_BaGroup($pearDB);
$optionsObj = new CentreonBam_Options($pearDB, $oreon->user->user_id);
$baObj = new CentreonBam_Ba($pearDB, null, $pearDBO);

$iconObj = new CentreonBam_Icon($pearDB);

$options = $optionsObj->getUserOptions();
$user_ba_list = $options['overview'];

$tab_ba_status = array(
                       0 => "OK", 
                       1 => "Warning", 
                       2 => "Critical", 
                       3 => "Unknown",
                       4 => "Pending"
                       );

$tab_ba_service = array("OK" => 'service_ok', "Warning" => 'service_warning', "Critical" => 'service_critical', "Unknown" => 'service_unknown', "Pending" => 'pending');

$buffer->startElement("root");
$buffer->writeElement("ba_label", _("Business Activity"));
$buffer->writeElement("ba_description", _("Description"));
$buffer->writeElement("reporting_period_label", _("Reporting Period"));
$buffer->writeElement("current_label", _("Current Level"));
$buffer->writeElement("duration_label", _("Duration"));
$buffer->writeElement("warning_label", _("Warning"));
$buffer->writeElement("critical_label", _("Critical"));
$buffer->writeElement("business_view_label", _("Business View"));
$buffer->writeElement("selected_bv", $bv_filter);
$buffer->writeElement("sid", $session_id);

// Group Selector
$query = "SELECT * FROM `mod_bam_ba_groups` WHERE visible = '1' ".$ba_acl->queryBuilder("AND", "id_ba_group", $ba_acl->getBaGroupStr())."ORDER BY ba_group_name";
$DBRES2 = $pearDB->query($query);
while ($rowb = $DBRES2->fetchRow()) {
	$buffer->startElement("bv");
    $buffer->writeElement("name", $rowb['ba_group_name']);
    $buffer->writeElement("ba_g_id", $rowb['id_ba_group']);
	$buffer->endElement();
}

// Filters
if (!$bv_filter) {
	$buffer->startElement("current_bv");
    $buffer->writeElement("name", _("Overview"));
    $buffer->writeElement("ba_g_id", "0");
	$buffer->endElement();
} else {
	$buffer->startElement("current_bv");
    $buffer->writeElement("name", $bvObj->getBaViewName($bv_filter));
    $buffer->writeElement("ba_g_id", $bv_filter);
	$buffer->endElement();
}

if (isset($_GET['limit']) && is_numeric($_GET['limit'])){
    $limit = $_GET['limit'];
} else {
    $query = "SELECT `value` FROM options WHERE `key` = 'maxViewMonitoring'";
    $DBRES = $pearDB->query($query);
    $row = $DBRES->fetchRow();
    $limit = $row['value'];
}

if (isset($_GET['num']) && is_numeric($_GET['num'])) {
    $num = $_GET['num'];
} else {
    $num = 0;
}

$orderTab = array();
$style = "list_two";

if ($bv_filter == 0) {
	$query = "SELECT SQL_CALC_FOUND_ROWS * FROM `mod_bam` ba "
        . "left join mod_bam_bagroup_ba_relation br on br.id_ba = ba.ba_id "
        . "WHERE ba.activate = '1' "
        . $ba_acl->queryBuilder("AND", "ba_id", $ba_acl->getBaStr())
        . " GROUP BY ba.ba_id "
        . "ORDER BY ba.current_level, ba.name "
        . "LIMIT ".($num * $limit).",".$limit;
} else {
	$query = "SELECT SQL_CALC_FOUND_ROWS * FROM `mod_bam` ba "
        . "WHERE activate = '1' "
        . "AND ba_id IN (SELECT id_ba FROM mod_bam_bagroup_ba_relation WHERE id_ba_group = '".$bv_filter."') "
        . $ba_acl->queryBuilder("AND", "ba_id", $ba_acl->getBaStr())
		. " ORDER BY ba.current_level, ba.name LIMIT ".($num * $limit).",".$limit;
}
$DBRES = $pearDB->query($query);
$numRows = $pearDB->numberRows();

$buffer->startElement("i");
$buffer->writeElement("numrows", $numRows);
$buffer->writeElement("num", $num);
$buffer->writeElement("limit", $limit);
$buffer->endElement();
while ($row = $DBRES->fetchRow()) {
    if (!$bv_filter && count($user_ba_list) && !isset($user_ba_list[$row['ba_id']])) {
        continue;
    }

    $baServiceDescription = 'ba_' . $row['ba_id'];

    $acknowledged = '0';
    $downtime = '0';

    $baHostId = $baObj->getCentreonHostBaId($row['ba_id']);
    $sql = 'SELECT name FROM hosts WHERE host_id = ' . $baHostId;
    $resHost = $pearDBO->query($sql);
    $baHostName = '';
    while ($dataHost = $resHost->fetchRow()) {
        $baHostName = $dataHost['name'];
    }

    $query2 = "SELECT s.acknowledged, s.scheduled_downtime_depth FROM services s, hosts h WHERE s.description = '" . $baServiceDescription . "' AND h.name = '" . $baHostName . "' AND s.host_id = h.host_id AND s.enabled = '1'";
    $DBRES2 = $pearDBO->query($query2);
    if ($DBRES2->numRows()) {
        $row2 = $DBRES2->fetchRow();
        $acknowledged = $row2['acknowledged'];
        $downtime = $row2['scheduled_downtime_depth'];
    }

    $buffer->startElement("ba");
    $buffer->writeElement("ba_id", $row['ba_id'], false);

    $hostId = $baObj->getCentreonHostBaId($row['ba_id']);
    $serviceId = $baObj->getCentreonServiceBaId($row['ba_id'], $hostId);

    $buffer->writeElement("host_id", $hostId);
    $buffer->writeElement("service_id", $serviceId);

    if (isset($row['id_ba_group'])) {
        $id_ba_group = $row['id_ba_group'];
    } else {
        $id_ba_group = "";
    }

    $buffer->writeElement("ba_g_id", $id_ba_group, false);
    $buffer->writeElement("name", $row['name'], false);
    $buffer->writeElement("description", $row['description'], false);
    $buffer->writeElement("current", $row['current_level'], false);
    $buffer->writeElement("warning", $row['level_w'], false);
    $buffer->writeElement("critical", $row['level_c'], false);
    if (isset($tab_ba_status[$row['current_status']])) {
        $status = $tab_ba_status[$row['current_status']];
    } else {
        $status = "";
    }
    $buffer->writeElement("status", $status, false);
    $buffer->writeElement("ack", $acknowledged, false);
    $buffer->writeElement("acknowledged", $acknowledged, false);
    $buffer->writeElement("downtime", $downtime, false);
    $buffer->writeElement("notif", $row["notifications_enabled"], false);
    $buffer->writeElement("reportingperiod", $tpObj->getTimePeriodName($row["id_reporting_period"]), false);

    $icon = $iconObj->getFullIconPath($row['icon_id']);
    if (is_null($icon)) {
        $buffer->writeElement("icon", "./modules/centreon-bam-server/core/common/img/kpi-service.png", false);
    } else {
        $buffer->writeElement("icon", $icon, false);
    }

    if ($row['last_state_change']) {
        $buffer->writeElement("duration", $durationObj->toString(time() - $row['last_state_change']), false);
    } else {
        $buffer->writeElement("duration", "N/A", false);
    }

    $svcIndex = getMyIndexGraph4Service($baHostName, $baServiceDescription, $pearDBO);
    $buffer->writeElement("svc_index", $svcIndex, false);
    $buffer->writeElement("ba_url", "./main.php?p=".$p."&o=d&ba_id=".$row['ba_id'], false);

    if (is_numeric($row["current_status"]) && isset($tab_ba_service[$tab_ba_status[$row["current_status"]]])) {
        $status_badge = $tab_ba_service[$tab_ba_status[$row["current_status"]]];
    } else {
        $status_badge = "pending";
    }

    $buffer->writeElement("status_badge", $status_badge, false);
	
    // Stytle
    if ($downtime != 0) {
        $style = "line_downtime";
    } else if ($acknowledged != 0) {
        $style = "line_ack";
    } else {
        $style == "list_two" ? $style = "list_one" : $style = "list_two";
    }

	$buffer->writeElement("tr_style", $style);
	$buffer->endElement();
}
$buffer->endElement();

// Send Header
header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');

// Write Buffer
$buffer->output();
