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
require_once ("./modules/centreon-bam-server/core/common/functions.php");

// Pear library
require_once "HTML/QuickForm.php";
require_once 'HTML/QuickForm/advmultiselect.php';
require_once 'HTML/QuickForm/Renderer/ArraySmarty.php';

$path = "./modules/centreon-bam-server/core/reporting/template";

$pearDB = new CentreonBam_Db();
$pearDBO = new CentreonBam_Db("centstorage");
$ba_meth = new CentreonBam_Ba($pearDB, null, $pearDBO);
$ba_acl_meth = new CentreonBam_Acl($pearDB, $oreon->user->user_id);
$report_meth = new CentreonBam_Reporting($pearDB, $pearDBO);
$tpOBj = new CentreonBam_TimePeriod($pearDB);

/*
 *  Getting service to report
 */
isset($_GET["item"]) ? $bam_id = $_GET["item"] : $bam_id = "NULL";
isset($_POST["item"]) ? $bam_id = $_POST["item"] : $bam_id;

require_once ("./modules/centreon-bam-server/core/reporting/javascript/reporting_js.php");

/*
 * QuickForm templates
 */
$attrsText 		= array("size"=>"30");
$attrsText2 	= array("size"=>"60");
$attrsAdvSelect = array("style" => "width: 200px; height: 100px;");
$attrsTextarea 	= array("rows"=>"5", "cols"=>"40");

/* Smarty template initialization */
$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl, "");
$tpl->assign('o', $o);

/*
 *  Assign centreon path
 */
$tpl->assign("centreon_path", $centreon_path);

/*
 * Translations and styles
 */
$oreon->optGen["color_undetermined"] = "#bcbdc0";

$tpl->assign('style_ok', "class='ListColCenter state_badge' style='background:#88b917'");
$tpl->assign('style_ok_alert', "class='ListColCenter' style='width: 25px; background:#88b917'");
$tpl->assign('style_warning' , "class='ListColCenter state_badge' style='background:#ff9a13'");
$tpl->assign('style_warning_alert' , "class='ListColCenter' style='width: 25px; background:#ff9a13'");
$tpl->assign('style_critical' , "class='ListColCenter state_badge' style='background:#e00b3d'");
$tpl->assign('style_critical_alert' , "class='ListColCenter' style='width: 25px; background:#e00b3d'");
$tpl->assign('style_unknown' , "class='ListColCenter state_badge' style='background:#bcbdc0'");
$tpl->assign('style_unknown_alert' , "class='ListColCenter' style='width: 25px; background:#bcbdc0'");
$tpl->assign('style_pending' , "class='ListColCenter state_badge' style='background:#D1D2D4'");
$tpl->assign('style_pending_alert' , "class='ListColCenter' style='width: 25px; background:#D1D2D4'");

$tpl->assign('actualTitle', _(" Actual "));

$tpl->assign('serviceTitle', _("Service"));
$tpl->assign('hostTitle', _("Host"));
$tpl->assign("allTilte",  _("All"));
$tpl->assign("averageTilte",  _("Average"));

$tpl->assign('OKTitle', _("Available"));
$tpl->assign('WarningTitle', _("Warning"));
$tpl->assign('UnknownTitle', _("Unknown"));
$tpl->assign('CriticalTitle', _("Critical"));
$tpl->assign('PendingTitle', _("Undetermined"));

$tpl->assign('stateLabel', _("Status"));
$tpl->assign('totalLabel', _("Total"));
$tpl->assign('durationLabel', _("Duration"));
$tpl->assign('totalTimeLabel', _("Total Time"));
$tpl->assign('meanTimeLabel', _("Mean Time"));
$tpl->assign('alertsLabel', _("Alerts"));

$tpl->assign('informationLabel', _("Information"));
$tpl->assign('report_period_label', _("Reporting period used by engine"));

$tpl->assign('DateTitle', _("Date"));
$tpl->assign('EventTitle', _("Event"));
$tpl->assign('InformationsTitle', _("Info"));

$tpl->assign('periodTitle', _("Period Selection"));
$tpl->assign('resumeTitle', _("Business Activity states"));
$tpl->assign('logTitle', _("Today's Host log"));
$tpl->assign('svcTitle', _("State Breakdowns For Host Services"));

/*
 * Definition of status
 */
 $state["UP"] = _("UP");
 $state["DOWN"] = _("DOWN");
 $state["UNREACHABLE"] = _("UNREACHABLE");
 $state["UNDETERMINED"] = _("UNDETERMINED");
 $tpl->assign('states', $state);

/*
 * CSS Definition for status colors
 */
$style["UP"] = "class='ListColCenter' style='background:#88B917'";
$style["DOWN"] = "class='ListColCenter' style='background:#E00B3D'";
$style["UNREACHABLE"] = "class='ListColCenter' style='background:#818285'";
$style["UNDETERMINED"] = "class='ListColCenter' style='background:#D1D2D4'";
$tpl->assign('style', $style);

/*
 * Init Timeperiod List
 */

# Getting period table list to make the form period selection (today, this week etc.)
$periodList = $report_meth->getPeriodList();

$color = array();
$color["UNKNOWN"] = "#bcbdc0";
$color["UP"] = "#88b917";
$color["DOWN"] = "#e00b3d";
$color["UNREACHABLE"] = "#818285";
$tpl->assign('color', $color);

/*
 * Getting timeperiod by day (example : 9:30 to 19:30 on monday,tue,wed,thu,fri)
 */
$reportingTimePeriod = $report_meth->getreportingTimePeriod();

/*
 * CSV export parameters
 */
$var_url_export_csv = "";


/*
 * setting variables for link with services
 */
$period = (isset($_POST["period"])) ? $_POST["period"] : "";
$period = (isset($_GET["period"])) ? $_GET["period"] : $period;
$get_date_start = (isset($_POST["StartDate"])) ? $_POST["StartDate"] : "";
$get_date_start = (isset($_GET["start"])) ? $_GET["start"] : $get_date_start;
$get_date_end = (isset($_POST["EndDate"])) ? $_POST["EndDate"] : "";
$get_date_end = (isset($_GET["end"])) ? $_GET["end"] : $get_date_end;
if ($get_date_start == "" && $get_date_end == "" && $period == "") {
    $period = "yesterday";
}
$tpl->assign("get_date_start", $get_date_start);
$tpl->assign("get_date_end", $get_date_end);
$tpl->assign("get_period", $period);

/*
 * Period Selection form
 */
$formPeriod = new HTML_QuickForm('FormPeriod', 'post', "?p=".$p);
$formPeriod->addElement('hidden', 'timeline', "1");
$formPeriod->addElement('header', 'title', _("Custom selection"));
$formPeriod->addElement('text', 'start', _("Begin date"));
$formPeriod->addElement('button', "startD", _("Modify"), array('class'=>'btc bt_success','style'=>'vertical-align: middle;'));
$formPeriod->addElement('text', 'end', _("End date"));
$formPeriod->addElement('button', "endD", _("Modify"), array('class'=>'btc bt_success','style'=>'vertical-align: middle;'));
$formPeriod->addElement('submit', 'submit', _("Apply"));
$formPeriod->setDefaults(array('period' => $period, "StartDate" => $get_date_start, "EndDate" => $get_date_end));
$formPeriod->addElement('select', 'period', "", $periodList);

/*
 * FORMS
 */

/* service Selection */
$items = array(NULL => NULL);
if (!$oreon->user->admin) {
    $items2  = $ba_acl_meth->getBa();
    foreach ($items2 as $key => $value) {
        $items[$key] = $ba_meth->getBA_Name($key);
    }
}
else {
    $items2 = $ba_meth->getBA_list();
    foreach ($items2 as $key => $value) {
        $items[$key] = $value;
    }
}
asort($items);
$form = new HTML_QuickForm('formItem', 'post', "?p=".$p);
$form->addElement('hidden', 'p', $p);
$redirect = $form->addElement('hidden', 'o');
$redirect->setValue($o);
$select = $form->addElement('select', 'item', _("Business Activity"), $items, array("onChange" =>"this.form.submit();"));
$form->addElement('hidden', 'period', $period);
$form->addElement('hidden', 'start', $get_date_start);
$form->addElement('hidden', 'end', $get_date_end);
/* Set service id with period selection form */
if ($bam_id != "NULL") {
    $formPeriod->addElement('hidden', 'item', $bam_id);
    $form->setDefaults(array('item' => $bam_id));
}

/* page id */
$tpl->assign('p', $p);
/*
 * END OF FORMS
 */

/*
 * Stats Display for selected service
 */
if (isset($bam_id) && $bam_id != "NULL") {
    /*
     * Getting periods values
     */
    $dates = $report_meth->getPeriodToReport();
    $start_date = $dates[0];
    $end_date = $dates[1];

    $serviceStats = array();
    $serviceStats = $report_meth->getbaLogInDb($bam_id, $start_date, $end_date, $reportingTimePeriod) ;
    /* Flash chart data */
    $pie_chart_get_str =  "&value[ok]=".$serviceStats["OK_TP"]."&value[warning]=".
                            $serviceStats["WARNING_TP"]."&value[critical]=".$serviceStats["CRITICAL_TP"]."&value[undetermined]=".$serviceStats["UNDETERMINED_TP"];

    $formPeriod->addElement('text', 'StartDate', _("From"), array("size"=>10, "id"=>"StartDate"));
    $formPeriod->addElement('text', 'EndDate', _("to"), array("size"=>10, "id"=>"EndDate"));
    $formPeriod->addElement('submit', 'button', _("Apply"), array('class'=>'btc bt_success','style'=>'vertical-align: middle;'));

    /* Exporting variables for ihtml */
    $tpl->assign('name', $items[$bam_id]);
    $tpl->assign('value_ok', $serviceStats["OK_TP"]);
    $tpl->assign('value_warning', $serviceStats["WARNING_TP"]);
    $tpl->assign('value_critical', $serviceStats["CRITICAL_TP"]);
    $tpl->assign('value_undetermined', $serviceStats["UNDETERMINED_TP"]);
    $tpl->assign('totalAlert', $serviceStats["TOTAL_ALERTS"]);
    $tpl->assign('totalTime',  $serviceStats["TOTAL_TIME_F"]);
    $tpl->assign('summary',  $serviceStats);
    $tpl->assign('date_start', date("d/m/Y H:i", $start_date));
    $tpl->assign('to', _(" to "));
    $tpl->assign('date_end', date("d/m/Y H:i", $end_date));

    $tpl->assign("link_csv_url", "./modules/centreon-bam-server/core/reporting/exportCSV.php?sid=".$sid."&ba_id=".$bam_id."&start=".$start_date."&end=".$end_date);
    $tpl->assign("link_csv_name", _("Export in CSV format"));

    $tpl->assign('periodTitle', _("Reporting Interval"));
    $tpl->assign('periodORlabel', _("&nbsp;|&nbsp;"));

    $query = "SELECT id_reporting_period FROM `mod_bam` WHERE ba_id = '".$bam_id."' LIMIT 1";

    $DBRES = $pearDB->query($query);
    if ($DBRES->numRows()) {
        $row = $DBRES->fetchRow();
        $report_tp_id = 0;
        if (isset($row['id_reporting_period']) && $row['id_reporting_period']) {
            $report_tp_id = $row['id_reporting_period'];
        }
        $tpl->assign("report_period", $tpOBj->getTodayTimePeriod($report_tp_id));
    }

    $formPeriod->setDefaults(array('period' => $period));
    $tpl->assign('id', $bam_id);
}

/*
 * Rendering forms
 */
$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
$formPeriod->accept($renderer);
$tpl->assign('formPeriod', $renderer->toArray());
$tpl->assign('please_select', _("Please select a Business Activity"));
$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
$form->accept($renderer);
$tpl->assign('formItem', $renderer->toArray());
$period_choice = (isset($_POST["period_choice"])) ? $_POST["period_choice"] : "";


$tpl->assign('period_choice', $period_choice);
$tpl->display("reporting.ihtml");

?>
<script type="text/javascript">
	initTimeline();

	// Selecting the default preset period
	if (jQuery( "input:[value='custom'][name='period_choice'][type='radio']").is(':checked') == false) {
		jQuery( "input:[value='on'][name='period_choice'][type='radio']").prop('checked', true);
	}
    jQuery( "select[name='period']" ).click(function() {
        jQuery( "input:not([value])[name='period_choice'][type='radio']").prop('checked', true);
    });
</script>
