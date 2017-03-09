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
    require_once($centreon_path . "www/class/centreonDuration.class.php");
    $durationObj = new CentreonDuration();
}
require_once("../../common/header.php");

$pearDB = new CentreonBam_Db();
$pearDBO = new CentreonBam_Db("centstorage");
$buffer = new CentreonBam_Xml();

$buffer->startElement("data");

$state["OK"] = _("OK");
$state["WARNING"] = _("WARNING");
$state["CRITICAL"] = _("CRITICAL");
$state["UNDETERMINED"] = _("UNDETERMINED");

if (isset($_GET["id"]) && isset($_GET["color"])) {
    $color = array();
    $get_color = $_GET["color"];

    foreach ($get_color as $key => $value) {
        $color[$key] = $value;
    }
    
    $request = "SELECT
                   `available` as OKTimeScheduled,
                   `degraded` as WARNINGTimeScheduled, `alert_degraded_opened` as WARNINGnbEvent,
                   `unavailable` as CRITICALTimeScheduled, `alert_unavailable_opened` as CRITICALnbEvent,
                   time_id
                FROM `mod_bam_reporting_ba_availabilities`
                WHERE ba_id LIKE '".(int)$_GET["id"]."' AND timeperiod_is_default = 1 ORDER BY `time_id` DESC";
    $DBRESULT = $pearDBO->query($request);

    $statesTab = array("OK", "WARNING", "CRITICAL");
    while ($row = $DBRESULT->fetchRow()) {
        $statTab = array();
        $totalTime = 0;
        $sumTime = 0;
        foreach ($statesTab as $key => $value) {
            if (isset($row[$value."TimeScheduled"])) {
                $statTab[$value."_T"] = $row[$value."TimeScheduled"];
                $totalTime += $row[$value."TimeScheduled"];
            } else {
                $statTab[$value."_T"] = 0;
            }
            if (isset($row[$value."nbEvent"])) {
                $statTab[$value."_A"] = $row[$value."nbEvent"];
            } else {
                $statTab[$value."_A"] = 0;
            }
        }
        
        $date_start = $row["time_id"];
        $date_end = $date_start + $totalTime;
        foreach ($statesTab as $key => $value) {
            if ($totalTime) {
                $statTab[$value."_MP"] = round(($statTab[$value."_T"] / ($totalTime) * 100),2);
            } else {
                $statTab[$value."_MP"] = 0;
            }
        }
        $detailPopup = '{table class=bulleDashtab}';
        $detailPopup .= '	{tr}{td class=bulleDashleft colspan=3}Day: '. date("d/m/Y", $date_start) .' --  Duration: '.$durationObj->toString($totalTime).'{/td}{td class=bulleDashleft }Alert{/td}{/tr}';
        foreach($statesTab as $key => $value) {
            $detailPopup .= '	{tr}' .
                '		{td class=bulleDashleft style="background:'.$color[$value].';"  }'._($value).':{/td}' .
                '		{td class=bulleDash}'. $durationObj->toString($statTab[$value."_T"]) .'{/td}' .
                '		{td class=bulleDash}'.$statTab[$value."_MP"].'%{/td}'.
                '		{td class=bulleDash}'.$statTab[$value."_A"].'{/td}';
            $detailPopup .= '	{/tr}';
        }
        $detailPopup .= '{/table}';

        
        $t = $totalTime;
        $t = round(($t - ($t * 0.11574074074)),2);
        
        foreach ($statesTab as $key => $value) {
            if ($statTab[$value."_MP"] > 0) {
                $day = date("d", $date_start);
                $year = date("Y", $date_start);
                $month = date("m", $date_start);
                $start = mktime(0, 0, 0, $month, $day, $year);
                $start += ($statTab[$value."_T"]/100*2);
                $end = $start + ($statTab[$value."_T"]/100*96);
                $buffer->startElement("event");
                $buffer->writeAttribute("start", create_date_timeline_format($start) . " GMT");
                $buffer->writeAttribute("end", create_date_timeline_format($end) . " GMT");
                $buffer->writeAttribute("color", $color[$value]);
                $buffer->writeAttribute("isDuration", "true");
                $buffer->writeAttribute("title", $statTab[$value."_MP"]."%");
                $buffer->text($detailPopup, false);
                $buffer->endElement();
            }
        }
    }
} else {
    $buffer->writeElement("error", "error");
}
$buffer->endElement();

// Send Header
header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');

$buffer->output();

