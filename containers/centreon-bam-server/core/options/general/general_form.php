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

$path = "./modules/centreon-bam-server/core/options/general/template";

$versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

$seconds = _("seconds of unavailability");

$attrsText 		= array("size"=>"30");
$attrsText2 	= array("size"=>"60");
$attrsTextSmall	= array("size"=>"5");
$attrsAdvSelect = array("style" => "width: 200px; height: 100px;");
$attrsTextarea 	= array("rows"=>"5", "cols"=>"40");
$template 		= "<table><tr><td>{unselected}</td><td align='center'>{add}<br><br><br>{remove}</td><td>{selected}</td></tr></table>";
$attrBtnSuccess = array();
$attrBtnDefault = array();
$multiSelectAdd = array('value' => _("Add"));
$multiSelectRemove = array('value' => _("Delete"));

if (!$versionDesign) {
    $attrBtnSuccess = array("class" => "btc bt_success");
    $attrBtnDefault = array("class" => "btc bt_default");
    $multiSelectAdd = array('value' => _("Add"), "class" => "btc bt_success");
    $multiSelectRemove = array('value' => _("Delete"), "class" => "btc bt_danger");
}

/* Lets retrieve data */
$data_tab = $ba_options->getGeneralOptions();

/* Basic info */
$form = new HTML_QuickForm('Form', 'post', "?p=".$p."&o=".$o);
$form->addElement('header', 'title', _("Default Settings"));

/* Criticity settings */
$form->addElement('text', 'weak_impact', _("Weak Impact"), $attrsTextSmall);
$form->addElement('text', 'minor_impact', _("Minor Impact"), $attrsTextSmall);
$form->addElement('text', 'major_impact', _("Major Impact"), $attrsTextSmall);
$form->addElement('text', 'critical_impact', _("Critical Impact"), $attrsTextSmall);
$form->addElement('text', 'blocking_impact', _("Blocking Impact"), $attrsTextSmall);

/* KPI Default Settings */
$form->addElement('text', 'kpi_warning_impact', _("Warning Business Impact"), $attrsTextSmall);
$form->addElement('text', 'kpi_critical_impact', _("Critical Business Impact"), $attrsTextSmall);
$form->addElement('text', 'kpi_unknown_impact', _("Unknown Business Impact"), $attrsTextSmall);
$form->addElement('text', 'kpi_boolean_impact', _("Boolean Business Impact"), $attrsTextSmall);

/* BA Default Settings */
// warning & critical threshold
$form->addElement('text', 'ba_warning_threshold', _("Monitoring Warning Threshold"), $attrsTextSmall);
$form->addElement('text', 'ba_critical_threshold', _("Monitoring Critical Threshold"), $attrsTextSmall);

// check time period
$tab_timeperiod = array();
$DBRESULT = $pearDB->query("SELECT tp_id, tp_name FROM timeperiod ORDER BY tp_name");
while($row = $DBRESULT->fetchRow())
	$tab_timeperiod[$row["tp_id"]] = htmlspecialchars($row["tp_name"]);

// reporting time period
$form->addElement('select', 'id_reporting_period', _("Reporting Time Period"), $tab_timeperiod);

// notif time period
$form->addElement('select', 'id_notif_period', _("Notification Time Period"), $tab_timeperiod);


$notifCmd = array();
$DBRESULT = $pearDB->query("SELECT command_id, command_name FROM command where command_type = 1");
while ($row = $DBRESULT->fetchRow()) {
	$notifCmd[$row["command_id"]] = $row["command_name"];
}
$form->addElement('select', 'command_id', _("Notification Command"), $notifCmd);

// notif contactgroups
$notifCgs = array();
$DBRESULT = $pearDB->query("SELECT cg_id, cg_name FROM contactgroup ORDER BY cg_name");
while ($row = $DBRESULT->fetchRow()) {
	$notifCgs[$row["cg_id"]] = $row["cg_name"];
}

$ams2 = $form->addElement('advmultiselect', 'bam_contact', _("Contact Groups"), $notifCgs, $attrsAdvSelect);
$ams2->setButtonAttributes('add', $multiSelectAdd);
$ams2->setButtonAttributes('remove', $multiSelectRemove);
$ams2->setElementTemplate($template);
echo $ams2->getElementJs(false);

// Notif interval
$form->addElement('text', 'notif_interval', _("Notification Interval"), $attrsTextSmall);

$subS = $form->addElement('submit', 'submitSaveChange', _("Save"), $attrBtnSuccess);
$form->setDefaults($data_tab);

/*
 *  Smarty template
 */
$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl);
$tpl->assign('o', $o);
$tpl->assign('p', $p);
$tpl->assign("img_kpi", "./modules/centreon-bam-server/core/common/images/gear.gif");
$tpl->assign("img_ba", "./modules/centreon-bam-server/core/common/images/tool.gif");
$tpl->assign("img_opt", "./modules/centreon-bam-server/core/common/images/wrench.png");
$tpl->assign("kpi_impact", _("Default impact settings"));
$tpl->assign("kpi_levels", _("Default Business Activity and Boolean rule impacts settings"));
$tpl->assign("ba_title", _("Business Activity Default Settings"));
$tpl->assign('time_unit', " * ".$oreon->Nagioscfg["interval_length"]." "._("seconds"));

$helptext = "";
include_once("help.php");
foreach ($help as $key => $text) {
    $helptext .= '<span style="display:none" id="help:'.$key.'">'.$text.'</span>'."\n";
}
$tpl->assign("helptext", $helptext);

$valid = false;
if ($form->validate())	{
	$opt = new CentreonBAM_Options($pearDB, $form);
	if ($form->getSubmitValue("submitSaveChange")) {
		$opt->setGeneralOptions();
	}
	$o = null;
	$form->addElement("button", "change", _("Modify"), array("onClick"=>"javascript:window.location.href='?p=".$p));
	$form->freeze();
	$valid = true;
}

$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
$renderer->setRequiredTemplate('{$label}&nbsp;<font color="red" size="1">*</font>');
$renderer->setErrorTemplate('<font color="red">{$error}</font><br />{$html}');
$form->accept($renderer);
$tpl->assign('form', $renderer->toArray());
if ($versionDisplay) {
    $tpl->display("general_form.ihtml");
} else {
    $tpl->display("general_form-2.7.ihtml");
}
?>
<script type='text/javascript' src='./modules/centreon-bam-server/core/common/javascript/initHelpTooltips.js'></script>
