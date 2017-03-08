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

require_once "../../centreon-bam-server.conf.php";
require_once $centreon_path . 'www/modules/centreon-bam-server/core/common/functions.php';
if (file_exists($centreon_path."www/class/centreonDuration.class.php")) {
    require_once $centreon_path . 'www/class/centreonDuration.class.php';
    $durationObj = new CentreonDuration();
}
require_once $centreon_path . 'www/modules/centreon-bam-server/core/common/header.php';

$pearDB = new CentreonBam_Db();
$pearDBO = new CentreonBam_Db("centstorage");
$ba = new CentreonBam_Ba($pearDB, null, $pearDBO);
$reporting = new CentreonBam_Reporting($pearDB, $pearDBO);

if (isset($_GET["sid"])) {
    $res = $pearDB->query("SELECT * FROM contact, session WHERE session.session_id='".$pearDB->escape($_GET['sid'])."' AND session.user_id = contact.contact_id");
    $sid = $_GET["sid"];
    $sid = htmlentities($sid);
    $res = $pearDB->query("SELECT * FROM session WHERE session_id = '".$pearDB->escape($sid)."'");
    if ($session = $res->fetchRow()) {
        $_POST["sid"] = $sid;
    } else {
        exit;
    }
} else {
    exit;
}

/* Getting BA ID */
isset ($_GET["ba_id"]) ? $ba_id = $_GET["ba_id"] : $ba_id = "NULL";

/*
 * Getting time interval to report
 */
//$dates = $reporting->getPeriodToReportForCSVExport();

$start_date = $_GET['start'];
$end_date = $_GET['end'];
$ba_name = $ba->getBA_Name($ba_id);

/*
 * file type setting
 */
header("Content-Type: application/csv-tab-delimited-table");
header("Content-disposition: filename=BA_" .$ba_name.".csv");

$csvStr = "";
$csvStr .= _("Business Activity").";"._("Begin date")."; "._("End date")."; "._("Duration")."\n";
$csvStr .= $ba_name."; ".date('Y-m-d', $start_date)."; ". date('Y-m-d', $end_date)."; ". $durationObj->toString($end_date - $start_date)."\n";
$csvStr .= "\n";

$csvStr .= _("Status").";"._("Time").";"._("Total Time").";"._("Mean Time")."; "._("Alert")."\n";
$reportingTimePeriod = $reporting->getreportingTimePeriod();
$serviceStats = $reporting->getbaLogInDb($ba_id, $start_date, $end_date, $reportingTimePeriod);
$csvStr .= "AVAILABLE;". $durationObj->toString($serviceStats["OK_T"]).";".$serviceStats["OK_TP"]."%;".$serviceStats["OK_MP"]. "%;-;\n";
$csvStr .= "WARNING;". $durationObj->toString($serviceStats["WARNING_T"]).";".$serviceStats["WARNING_TP"]."%;".$serviceStats["WARNING_MP"]. "%;".$serviceStats["WARNING_A"].";\n";
$csvStr .= "CRITICAL;". $durationObj->toString($serviceStats["CRITICAL_T"]).";".$serviceStats["CRITICAL_TP"]."%;".$serviceStats["CRITICAL_MP"]. "%;".$serviceStats["CRITICAL_A"].";\n";
$csvStr .= "UNDETERMINED;". $durationObj->toString($serviceStats["UNDETERMINED_T"]).";".$serviceStats["UNDETERMINED_TP"]."%;;;\n";
$csvStr .= "\n";
$csvStr .= "\n";

/*
 * Getting evolution of service stats in time
 */
$csvStr .= _("Day").";"._("Duration").";" .
           _("Available Time")."; "._("% Available")."; " . _("Available Alerts") . ";" .
           _("Warning Time")."; " . _("% Warning") . ";" ._("Warning Alerts") . ";" .
           _("Critical Time")."; " . _("% Critical") . ";" . _("Critical Alerts") . ";\n";

$request = "SELECT `time_id`, `available` AS OKTimeScheduled, `degraded` AS WARNINGTimeScheduled, `unavailable` AS CRITICALTimeScheduled " 
    . " FROM `mod_bam_reporting_ba_availabilities` "
    . " WHERE `ba_id` = '".$ba_id."' "
    . " and time_id BETWEEN ".$start_date." AND ".$end_date." "
    . " AND timeperiod_is_default = 1"
    . " ORDER BY `time_id` DESC ";
$DBRESULT = $pearDBO->query($request);
while ($row = $DBRESULT->fetchRow()) {
    $duration = $row["OK_MP"] + $row["WARNING_MP"] + $row["CRITICAL_MP"];
    
    /* Percentage by status */
    $duration = $row["OKTimeScheduled"] + $row["WARNINGTimeScheduled"] + $row["CRITICALTimeScheduled"];
    $row["OK_MP"] = round($row["OKTimeScheduled"] * 100 / $duration, 2);
    $row["WARNING_MP"] = round($row["WARNINGTimeScheduled"] * 100 / $duration, 2);
    $row["CRITICAL_MP"] = round($row["CRITICALTimeScheduled"] * 100 / $duration, 2);
    $csvStr .= date('Y-m-d', $row["time_id"]).";".$durationObj->toString($duration).";".
    $durationObj->toString($row["OKTimeScheduled"]).";".$row["OK_MP"]."%;".$row["OKnbEvent"].";".
    $durationObj->toString($row["WARNINGTimeScheduled"]).";".$row["WARNING_MP"]."%;".$row["WARNINGnbEvent"].";".
    $durationObj->toString($row["CRITICALTimeScheduled"]).";".$row["CRITICAL_MP"]."%;".$row["CRITICALnbEvent"].";\n";
}

echo html_entity_decode($csvStr);
