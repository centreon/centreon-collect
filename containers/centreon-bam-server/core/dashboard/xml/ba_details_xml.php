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

if (file_exists($centreon_path . "www/class/Session.class.php")) {
    require_once($centreon_path . "www/class/Session.class.php");
}
if (file_exists($centreon_path . "www/class/Oreon.class.php")) {
    require_once($centreon_path . "www/class/Oreon.class.php");
}
if (file_exists($centreon_path . "www/class/other.class.php")) {
    require_once($centreon_path . "www/class/other.class.php");
}

require_once("../../common/header.php");

if (!isset($_GET['sid']) || !isset($_GET['ba_id'])) {
    exit();
}

$session_id = $_GET['sid'];
$ba_id = $_GET['ba_id'];

$pearDB = new CentreonBam_Db();
$pearDBndo = new CentreonBam_Db("centstorage");
$ba_acl = new CentreonBam_Acl($pearDB, $oreon->user->user_id);
$buffer = new CentreonBam_XML();
$hObj = new CentreonBam_Host($pearDB);
$sObj = new CentreonBam_Service($pearDB);
$metaObj = new CentreonBam_Meta($pearDB);
$baObj = new CentreonBam_Ba($pearDB, null, $pearDBndo);
$kpiObj = new CentreonBam_Kpi($pearDB);
$boolObj = new CentreonBam_Boolean($pearDB);

$tab_kpi_type = array(0 => _("Regular Service"), 1 => _("Meta Service"), 2 => _("Business Activity"), 3 => _('Boolean KPI'));
$tab_ba_status = array(0 => "OK", 1 => "Warning", 2 => "Critical", 3 => "Unknown");
$tab_ba_service = array("OK" => "#88B917", "Warning" => "#ff9A13", "Critical" => "#E00B3D", "Unknown" => "#BCBDC0");

/* Let's get the kpi first */
$query = "SELECT id_ba, kpi_id, activate, kpi_type, config_type,
          drop_warning, drop_critical, drop_unknown, 
          drop_warning_impact_id, drop_critical_impact_id, drop_unknown_impact_id,
          host_id, service_id, id_indicator_ba, meta_id, boolean_id, current_status, last_impact
          FROM `mod_bam_kpi` kpi 
          WHERE id_ba = " . $pearDB->escape($ba_id) . " " .
        $ba_acl->queryBuilder("AND", "id_ba", $ba_acl->getBaStr()) .
        " AND activate = '1' 
          ORDER BY kpi_type ";
$DBRES = $pearDB->query($query);

$kpi_tab = array();
$svcStr = "";
$host_list_str = "";
$aSvc = array();
while ($row = $DBRES->fetchRow()) {
    if ($row['config_type']) {
        $kpi_tab[$row['kpi_id']]['warning_impact'] = $row['drop_warning'];
        $kpi_tab[$row['kpi_id']]['critical_impact'] = $row['drop_critical'];
        $kpi_tab[$row['kpi_id']]['unknown_impact'] = $row['drop_unknown'];
    } else {
        $kpi_tab[$row['kpi_id']]['warning_impact'] = $kpiObj->getCriticityImpact($row['drop_warning_impact_id']);
        $kpi_tab[$row['kpi_id']]['critical_impact'] = $kpiObj->getCriticityImpact($row['drop_critical_impact_id']);
        $kpi_tab[$row['kpi_id']]['unknown_impact'] = $kpiObj->getCriticityImpact($row['drop_unknown_impact_id']);
    }
    $kpi_tab[$row['kpi_id']]['type'] = $row['kpi_type'];

    $kpiId = $row['kpi_id'];
    switch ($row['kpi_type']) {
        case "0" :
            $hostId = $row['host_id'];
            if ($host_list_str != "") {
                $host_list_str .= ", ";
            }
            $host_list_str .= $hostId;
            $svcId = $row['service_id'];
            $kpi_tab[$kpiId]['url'] = "#";
            break;
        case "1" :
            $hostId = $metaObj->getCentreonHostMetaId($pearDBndo);
            $svcId = $metaObj->getCentreonServiceMetaId($row['meta_id'], $pearDBndo);
            $kpi_tab[$kpiId]['icon'] = "./img/icons/service.png";
            $kpi_tab[$kpiId]['url'] = "#";
            break;
        case "2" :
            $hostId = $baObj->getCentreonHostBaId($row['id_indicator_ba']);
            $svcId = $baObj->getCentreonServiceBaId($row['id_indicator_ba'], $hostId);
            $kpi_tab[$kpiId]['url'] = "javascript: showBADetails('" . $row['id_indicator_ba'] . "', 0);";
            $kpi_tab[$kpiId]['originalName'] = $baObj->getBA_Name($row['id_indicator_ba']);
            break;
        case "3" :
            $hName = "";
            $kpi_tab[$kpiId]['booleanData'] = $boolObj->getData($row['boolean_id']);
            $sName = $kpi_tab[$kpiId]['booleanData']['name'];
            $kpi_tab[$kpiId]['sName'] = $sName;
            $kpi_tab[$kpiId]['url'] = "#";
            break;
    }

    if (isset($hostId))
        $kpi_tab[$kpiId]['hostId'] = $hostId;
    if (isset($svcId) && $svcId > 0) {
        $kpi_tab[$kpiId]['svcId'] = $svcId;
        $aSvc[] = $svcId;
    }

    $kpi_tab[$kpiId]['current_state'] = $row['current_status'];
    $kpi_tab[$kpiId]['last_impact'] = $row['last_impact'];
}
if (count($aSvc) > 0) {
    $svcStr = implode(",", $aSvc);

    $query2 = "SELECT h.name as hName,
                      h.host_id as host_id,
                      s.description as sName,
                      s.service_id as service_id,
                      s.output,
                      s.last_hard_state as current_state,
                      h.state as hstate,
                      h.state_type as hstate_type,
                      s.last_check,
                      s.next_check,
                      s.last_check,
                      s.next_check,
                      s.last_state_change,
                      s.acknowledged as problem_has_been_acknowledged,
                      s.scheduled_downtime_depth,
                      s.icon_image
           FROM services s, hosts h
               WHERE s.host_id = h.host_id
               AND s.service_id IN (" . $svcStr . ")
           AND s.enabled = 1 AND h.enabled = 1
               ORDER BY hName, sName";
    $DBRES2 = $pearDBndo->query($query2);
}

$buffer->startElement("root");
$buffer->writeElement("ba_name", $baObj->getBA_Name($ba_id), false);
$buffer->writeElement("indicator_label", _("Indicator"));
$buffer->writeElement("ba_label", _("Business Activity"));
$buffer->writeElement("indicator_type_label", _("Type"));
$buffer->writeElement("output_label", _("Output"));
$buffer->writeElement("status_label", _("Status"));
$buffer->writeElement("impact_label", _("Impact"));
$buffer->writeElement("goBackLabel", _("Go back"));
$buffer->writeElement("sid", $session_id);

$style = "list_two";
$orderTab = array();
$orderImpact = array();

foreach ($kpi_tab as $key => $value) {
    $key = trim($key);
    if ($kpi_tab[$key]['type'] == 3) {
        $orderTab[$key]["id"] = $key;
        $orderTab[$key]["ba_id"] = $key;
        $orderTab[$key]["status"] = $tab_ba_status[0];
        if ($kpi_tab[$key]['current_state']) {
            $exprReturnValue = $kpi_tab[$key]['booleanData']['bool_state'];
        } else {
            $exprReturnValue = $kpi_tab[$key]['booleanData']['bool_state'] ? 0 : 1;
        }
        $orderTab[$key]["output"] = sprintf(_('This KPI returns %s'), $exprReturnValue ? _('True') : _('False'));
        $orderTab[$key]["type_string"] = $tab_kpi_type[$kpi_tab[$key]['type']];
        $orderTab[$key]["type"] = $kpi_tab[$key]['type'];
        $orderTab[$key]["icon"] = "./modules/centreon-bam-server/core/common/images/dot-chart.gif";
        $orderTab[$key]["url"] = $kpi_tab[$key]['url'];
        $orderTab[$key]["hname"] = "";
        $orderTab[$key]["sname"] = $kpi_tab[$key]['sName'];
        $orderTab[$key]["ack"] = "";
        $orderTab[$key]["downtime"] = "---";
        $orderTab[$key]["warning_impact"] = "";
        $orderTab[$key]["critical_impact"] = $kpi_tab[$key]['critical_impact'];
        $orderTab[$key]["unknown_impact"] = "";
        $orderTab[$key]["last_impact"] = $kpi_tab[$key]['last_impact'];
        $impact = $kpi_tab[$key]['last_impact'];
        $orderTab[$key]["impact"] = $impact;

        if ($kpi_tab[$key]['current_state']) {
            $orderTab[$key]["status"] = $tab_ba_status[2];
        }
        $orderTab[$key]["status_color"] = $tab_ba_service[$orderTab[$key]["status"]];
        $orderTab[$key]["ba_action"] = "";
        $orderImpact[$key] = $orderTab[$key]["last_impact"];
        unset($kpi_tab[$key]);
    }
}
if (count($aSvc) > 0) {
    while ($row2 = $DBRES2->fetchRow()) {
        foreach ($kpi_tab as $key => $value) {
            $key = trim($key);
            if (isset($kpi_tab[$key]) && $kpi_tab[$key]['hostId'] == $row2['host_id'] && $kpi_tab[$key]['svcId'] == $row2['service_id']) {
                $orderTab[$key]["id"] = $key;
                $orderTab[$key]["ba_id"] = $key;
                $orderTab[$key]["status"] = $tab_ba_status[$kpi_tab[$key]['current_state']];
                $orderTab[$key]["output"] = $row2['output'];
                $orderTab[$key]["type_string"] = $tab_kpi_type[$kpi_tab[$key]['type']];
                $orderTab[$key]["type"] = $kpi_tab[$key]['type'];
                if ($row2['icon_image'] == '') {
                    $orderTab[$key]["icon"] = './img/icons/service.png';
                } else {
                    $orderTab[$key]["icon"] = './img/media/' . $row2['icon_image'];
                }
                $orderTab[$key]["url"] = $kpi_tab[$key]['url'];
                $orderTab[$key]["hname"] = $row2['hName'];
                $orderTab[$key]["ack"] = $row2['problem_has_been_acknowledged'];
                $orderTab[$key]["downtime"] = $row2['scheduled_downtime_depth'];
                $orderTab[$key]["warning_impact"] = $kpi_tab[$key]['warning_impact'];
                $orderTab[$key]["critical_impact"] = $kpi_tab[$key]['critical_impact'];
                $orderTab[$key]["unknown_impact"] = $kpi_tab[$key]['unknown_impact'];
                $orderTab[$key]["status_color"] = $tab_ba_service[$tab_ba_status[$kpi_tab[$key]['current_state']]];
                $impact = $kpi_tab[$key]["last_impact"];
                $orderTab[$key]["impact"] = $impact;
                $link = "";
                $orderTab[$key]["sname"] = $row2['sName'];
                if ($orderTab[$key]["type"] == "2") {
                    $link = "javascript:updateGraph('" . $orderTab[$key]["sname"] . "')";
                    $orderTab[$key]["sname"] = $kpi_tab[$key]['originalName'];
                }
                $orderTab[$key]["ba_action"] = $link;
                $orderImpact[$key] = $impact;
                unset($kpi_tab[$key]);
            }
        }
    }
}
arsort($orderImpact);
$style = "list_two";
$count = 0;
foreach ($orderImpact as $key => $value) {
    $buffer->startElement("kpi");
    foreach ($orderTab[$key] as $key2 => $value2) {
        if ($key2 != "type_string") {
            $buffer->writeElement($key2, $value2, false);
        } else {
            $buffer->writeElement($key2, $value2);
        }
    }

    if ($orderTab[$key]['downtime'] != 0) {
        $style = "line_downtime";
    } else if ($orderTab[$key]['ack'] != 0) {
        $style = "line_ack";
    } else {
        $style == "list_one" ? $style = "list_two" : $style = "list_one";
    }
    $buffer->writeElement("tr_style", $style);
    $buffer->endElement();
    if ($count >= 20) {
        break;
    }
    $count++;
}

$buffer->endElement();

// Send Header
header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');

// Display
$buffer->output();
