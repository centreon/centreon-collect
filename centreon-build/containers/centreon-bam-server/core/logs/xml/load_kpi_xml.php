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
if (file_exists($centreon_path . "www/class/Oreon.class.php")) {
    require_once($centreon_path . "www/class/Oreon.class.php");
}
if (file_exists($centreon_path . "www/class/Session.class.php")) {
    require_once($centreon_path . "www/class/Session.class.php");
}
if (file_exists($centreon_path . "www/class/other.class.php")) {
    require_once($centreon_path . "www/class/other.class.php");
}

require_once($centreon_path . "www/modules/centreon-bam-server/core/common/header.php");
require_once($centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Xml.php");
require_once($centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Db.php");
require_once($centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/Acl.php");
require_once($centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/Ba.php");

if (!isset($_GET['sid']) || !isset($_GET['datetime']) || !isset($_GET['ba_id'])) {
    exit();
}

isset($_GET['periodz']) ? $periodz = $_GET['periodz'] : $periodz = "today";

$session_id = $_GET['sid'];
$datetime = $_GET['datetime'];
$ba_id = $_GET['ba_id'];

$pearDB = new CentreonBam_Db();
$pearDBO = new CentreonBam_Db("centstorage");
$buffer = new CentreonBam_Xml();
$baObj = new CentreonBam_Ba($pearDB, null, $pearDBO);

$tab_ba_status = array(
    0 => "OK", 
    1 => "Warning", 
    2 => "Critical", 
    3 => "Unknown"
);

$tab_ba_service = array(
    "OK" => "#88B917", 
    "Warning" => "#FF9A13",
    "Critical" => "#E00B3D",
    "Unknown" => "#BCBDC0"
);

$tab_ba_bagde = array(
    0 => "badge service_ok",
    1 => "badge service_warning",
    2 => "badge service_critical",
    3 => "badge service_unknown"
);


$tab_kpi_type = array(
    0 => _('Regular Service'),
    1 => _('Meta Service'),
    2 => _('Business Activity'),
    3 => _('Boolean')
);

$buffer->startElement("root");
$buffer->writeElement("kpi_label", _("Key Performance Indicators"));
$buffer->writeElement("type_label", _("KPI type"));
$buffer->writeElement("status_label", _("Status"));
$buffer->writeElement("impact_label", _("Impact"));
$buffer->writeElement("downtime_label", _("In downtime"));
$buffer->writeElement("output_label", _("Output"));
$buffer->writeElement("start_time_label", _("Start time"));
$buffer->writeElement("end_time_label", _("End time"));
$buffer->writeElement("sid", $session_id);


$log_meth = new CentreonBAM_Log($pearDB, $pearDBO, $ba_id);
$dates = $log_meth->getPeriodToReport($periodz);

/*
$buffer->writeElement("start", $dates[0]);
$buffer->writeElement("end", $dates[1]);
*/





$datetab = explode(" ", $datetime);
$tab1 = explode("/", $datetab[0]);
$year = $tab1[2];
$month = $tab1[0];
$day = $tab1[1];
$hour = $datetab[1][0] . $datetab[1][1];
$minute = $datetab[1][3] . $datetab[1][4];
$seconds = $datetab[1][6] . $datetab[1][7];
$unixtime = mktime($hour, $minute, $seconds, $month, $day, $year);

$query = "SELECT kpi.kpi_name, klog.start_time, klog.end_time, klog.first_output, 
    klog.status, klog.in_downtime, klog.impact_level, kpi.host_name, kpi.service_description,
    kpi.service_id, kpi.meta_service_id, kpi.kpi_ba_id, kpi.boolean_id
    FROM mod_bam_reporting_kpi kpi, mod_bam_reporting_kpi_events klog, 
    mod_bam_reporting_relations_ba_kpi_events rel, mod_bam_reporting_ba_events blog
    WHERE kpi.kpi_id = klog.kpi_id
    AND klog.kpi_event_id = rel.kpi_event_id
    AND rel.ba_event_id = blog.ba_event_id
    AND blog.ba_id = kpi.ba_id
    AND kpi.ba_id = " . $pearDBO->escape($ba_id) . "
    AND blog.start_time = " . $pearDBO->escape($unixtime) . "
    AND (klog.start_time BETWEEN " . $pearDBO->escape($dates[0]) . " 
	AND " . $pearDBO->escape($dates[1]) . ")

    ORDER BY klog.start_time DESC";

$DBRES = $pearDBO->query($query);

$nb = "0";
if ($DBRES->numRows()) {
    $nb = "1";
}
$buffer->writeElement("nb", $nb);

$trStyle = "list_two";
while ($row = $DBRES->fetchRow()) {
    $buffer->startElement("kpi");
    $buffer->writeElement("start_time", date("d/m/Y H:i", $row['start_time']));
    $buffer->writeElement("end_time", $row['end_time'] ? date("d/m/Y H:i", $row['end_time']) : ' - ');
    $buffer->writeElement("output", $row['first_output']);
    $buffer->writeElement("status", $tab_ba_status[$row['status']]);
    $buffer->writeElement("impact", $row['impact_level'] . "%");
    $buffer->writeElement("in_downtime", $row['in_downtime'] ? _("Yes") : _("No"));
    $type = 0; 
    if ($row['meta_service_id']) {
        $type = 1;
    } elseif ($row['kpi_ba_id']) {
        $type = 2;
    } elseif ($row['boolean_id']) {
        $type = 3;
    }
    $buffer->writeElement("type", $tab_kpi_type[$type]);
    if ($type == 0) {
        $buffer->writeElement("name", $row['host_name'] . ' - ' . $row['service_description']);
    } else {
        $buffer->writeElement("name", slashConvert($row['kpi_name']));
    }
    $buffer->writeElement("status_color", $tab_ba_service[$tab_ba_status[$row['status']]]);
    $buffer->writeElement("badge", $tab_ba_bagde[$row['status']]);
    $buffer->writeElement("tr_style", $trStyle == "list_one" ? $trStyle = "list_two" : $trStyle = "list_one");
    $buffer->endElement();
}

$buffer->endElement();

header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');
$buffer->output();
?>
