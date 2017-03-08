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
require_once ("./modules/centreon-bam-server/centreon-bam-server.conf.php");
require_once ("./modules/centreon-bam-server/core/common/functions.php");
require_once ("./modules/centreon-bam-server/core/configuration/ba/rulesFunctions.php");

$path = "./modules/centreon-bam-server/core/configuration/ba/template";

// Ajax Flow
$attrCommands = array(
    'datasourceOrigin' => 'ajax',
    'multiple' => false,
    'linkedObject' => 'centreonCommand'
);

$versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

// Case of modification
if ($o == "c" && isset($id_ba)) {
	$query = "SELECT * FROM `mod_bam` WHERE ba_id = '".$id_ba."'";
	$DBRES = $pearDB->query($query);
	$data_tab = array();
	while ($row = $DBRES->fetchRow()) {
		/* Let's fill the fields */
		$data_tab['ba_name'] = $row['name'];
		$data_tab['ba_desc'] = $row['description'];
		$data_tab['ba_id'] = $row['ba_id'];
		$data_tab['ba_warning'] = $row['level_w'];
		$data_tab['ba_critical'] = $row['level_c'];
		$data_tab['id_notif_period'] = $row['id_notification_period'];
		$data_tab['id_reporting_period'] = $row['id_reporting_period'];
		$data_tab['notif_interval'] = $row['notification_interval'];
		$data_tab['notifications_enabled'] = $row['notifications_enabled'];
		$data_tab['bam_comment'] = $row['comment'];
		$data_tab['bam_activate'] = $row['activate'];
        $data_tab['inherit_kpi_downtimes'] = $row['inherit_kpi_downtimes'];
		$data_tab['icon'] = $row['icon_id'];
		$data_tab['notifOpts'] = explode(',', $row['notification_options']);
		$data_tab['sla_month_percent_warn'] = $row['sla_month_percent_warn'];
		$data_tab['sla_month_percent_crit'] = $row['sla_month_percent_crit'];
		$data_tab['sla_month_duration_warn'] = $row['sla_month_duration_warn'];
		$data_tab['sla_month_duration_crit'] = $row['sla_month_duration_crit'];
		foreach ($data_tab["notifOpts"] as $key => $value) {
			$data_tab["notifOpts"][trim($value)] = 1;
		}
       	$data_tab['event_handler_enabled'] = $row['event_handler_enabled'];
        $data_tab['event_handler_command'] = $row['event_handler_command'];
        $data_tab['event_handler_args'] = $row['event_handler_args'];
	}

	// Set Contact Group notifications
	$DBRESULT = $pearDB->query("SELECT id_cg FROM mod_bam_cg_relation WHERE id_ba = '".$id_ba."'");
	while ($row = $DBRESULT->fetchRow()) {
		$data_tab["bam_contact"][] = $row['id_cg'];
	}

	// Set escalation
	$DBRESULT = $pearDB->query("SELECT id_esc FROM mod_bam_escal_relation WHERE id_ba = '".$id_ba."'");
	while ($row = $DBRESULT->fetchRow()) {
		$data_tab["bam_esc"][] = $row['id_esc'];
	}

	// Inherits parent
	$data_tab["inherits_parent"] = $row["inherits_parent"];

	// Set Notification Failure Criteria
	$data_tab['notification_failure_criteria'] = explode(',', $row['notification_failure_criteria']);
	foreach ($data_tab["notification_failure_criteria"] as $key => $value) {
		$data_tab["notification_failure_criteria"][trim($value)] = 1;
	}

	// Set Execution Failure Criteria
	$data_tab["execution_failure_criteria"] = explode(',', $row["execution_failure_criteria"]);
	foreach ($data_tab["execution_failure_criteria"] as $key => $value) {
		$data_tab["execution_failure_criteria"][trim($value)] = 1;
	}

	// Set BA group
	$DBRESULT = $pearDB->query("SELECT DISTINCT id_ba_group FROM mod_bam_bagroup_ba_relation WHERE id_ba = '".$id_ba."'");
	while ($row = $DBRESULT->fetchRow()) {
		$data_tab["ba_group_list"][] = $row["id_ba_group"];
	}

	// Set reporting time periods
	$res = $pearDB->query("SELECT DISTINCT tp_id FROM mod_bam_relations_ba_timeperiods WHERE ba_id = " . $pearDB->escape($id_ba));
	while ($row = $res->fetchRow()) {
		$data_tab['reporting_timeperiods'][] = $row['tp_id'];
	}
    
    // Get the poller id relation
    $res = $pearDB->query("SELECT b.poller_id
        FROM mod_bam_poller_relations b, nagios_server ns
        WHERE b.ba_id = " . $pearDB->escape($id_ba) . " AND ns.id = b.poller_id AND ns.localhost != '1'");
    $row = $res->fetchRow();
    $data_tab['additional_poller'] = 0;
    if ($row) {
        $data_tab['additional_poller'] = $row['poller_id'];
    }
} else {
	// Case of new entry, we still fill the fields with user's default value
	$options = new CentreonBAM_Options($pearDB, $oreon->user->user_id);
	$opt = $options->getGeneralOptions();

	$data_tab['ba_warning'] = $opt['ba_warning_threshold'];
	$data_tab['ba_critical'] = $opt['ba_critical_threshold'];
	$data_tab['id_notif_period'] = $opt['id_notif_period'];
	$data_tab['id_reporting_period'] = $opt['id_reporting_period'];
	$data_tab['notif_interval'] = $opt['notif_interval'];
	$data_tab['sla_warning'] = $opt['sla_warning'];
	$data_tab['sla_critical'] = $opt['sla_critical'];
	if (isset($opt['bam_contact']) && $opt['bam_contact']) {
		$data_tab["bam_contact"] = explode(",", $opt['bam_contact']);
	}
	$data_tab['notifications_enabled'] = '1';
}

$attrsText 		= array("size"=>"30");
$attrsText2 	= array("size"=>"60");
$attrsTextSmall = array("size"=>"5");
$attrsAdvSelect = array("style" => "width: 200px; height: 100px;");
$attrsTextarea 	= array("rows"=>"5", "cols"=>"40");
$template 		= "<table><tr><td>{unselected}</td><td align='center'>{add}<br><br><br>{remove}</td><td>{selected}</td></tr></table>";
$attrBtnSuccess = array();
$attrBtnDefault = array();
$multiSelectAdd = array('value' => _("Add"));
$multiSelectRemove = array('value' => _("Delete"));

$attrBtnSuccess = array("class" => "btc bt_success");
$attrBtnDefault = array("class" => "btc bt_default");
$multiSelectAdd = array('value' => _("Add"), "class" => "btc bt_success");
$multiSelectRemove = array('value' => _("Delete"), "class" => "btc bt_danger");

$form = new HTML_QuickForm('Form', 'post', "?p=".$p);
$form->addElement('header', 'title', _("Business Activity"));
$form->addElement('header', 'information', _("Information"));

/* Information */
$form->addElement('text', 'ba_name', _("Name"), $attrsText);
$form->addElement('text', 'ba_desc', _("Description"), $attrsText);
$form->addElement('text', 'ba_warning', _("Monitoring Warning Threshold"), $attrsTextSmall);
$form->addElement('text', 'ba_critical', _("Monitoring Critical Threshold"), $attrsTextSmall);

/* Reporting Time period */
$tab_timeperiod = array();
$DBRESULT = $pearDB->query("SELECT tp_id, tp_name FROM timeperiod ORDER BY tp_name");
while ($row = $DBRESULT->fetchRow()) {
	$tab_timeperiod[$row["tp_id"]] = $row["tp_name"];
}
$form->addElement('select', 'id_reporting_period', _("Default reporting time periods used by Centreon BAM and Centreon BI"), $tab_timeperiod);

/* Notification */
// Contact Groups
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

// Notification Time Period
$form->addElement('select', 'id_notif_period', _("Notification Time Period"), $tab_timeperiod);

// Notification Interval
$form->addElement('text', 'notif_interval', _("Notification Interval"), $attrsTextSmall);

// Notification Options
$bamNotifOpt = array();
$bamNotifOpt[] = &HTML_QuickForm::createElement('checkbox', 'r', '&nbsp;', 'Recovery');
$bamNotifOpt[] = &HTML_QuickForm::createElement('checkbox', 'w', '&nbsp;', 'Warning');
$bamNotifOpt[] = &HTML_QuickForm::createElement('checkbox', 'c', '&nbsp;', 'Critical');
$bamNotifOpt[] = &HTML_QuickForm::createElement('checkbox', 'f', '&nbsp;', 'Flapping');
$form->addGroup($bamNotifOpt, 'notifOpts', _("Notification Options"), '&nbsp;&nbsp;');

// Notification Enabled
$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'notifications_enabled', null, _("Yes"), '1');
$tab[] = &HTML_QuickForm::createElement('radio', 'notifications_enabled', null, _("No"), '0');
$form->addGroup($tab, 'notifications_enabled', _("Enable notification"), '&nbsp;');

/* Settings */

/* Additionnal poller */
$res = $pearDB->query("SELECT id, name FROM nagios_server WHERE localhost = '0'");
$listPoller = array();
$listPoller[0] = "";
while ($row = $res->fetchRow()) {
    $listPoller[$row['id']] = $row['name'];
}
$form->addElement('select', 'additional_poller', _("Additional Poller"), $listPoller);

// Enable - Disable
$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'bam_activate', null, _("Enable"), '1');
$tab[] = &HTML_QuickForm::createElement('radio', 'bam_activate', null, _("Disable"), '0');
$form->addGroup($tab, 'bam_activate', _("Activate"), '&nbsp;');

// Inherit KPI downtimes
$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'inherit_kpi_downtimes', null, _("Yes"), 1);
$tab[] = &HTML_QuickForm::createElement('radio', 'inherit_kpi_downtimes', null, _("No"), 0);
$form->addGroup($tab, 'inherit_kpi_downtimes', _("Automatically inherit KPI downtimes ?"), '&nbsp;');

// Comments
$form->addElement('textarea', 'bam_comment', _("Comment"), $attrsTextarea);
if ($o == "a") {
	$form->setDefaults(array('ba_display' => '1'));
	$form->setDefaults(array('bam_activate' => '1'));
    $form->setDefaults(array('inherit_kpi_downtimes' => '0'));
}

/* Escalation */
$escalations = array();
$DBRESULT = $pearDB->query("SELECT esc_id, esc_name FROM escalation ORDER BY esc_name");
while ($row = $DBRESULT->fetchRow()) {
	$escalations[$row["esc_id"]] = $row["esc_name"];
}

$ams1 = $form->addElement('advmultiselect', 'bam_esc', _("Escalations"), $escalations, $attrsAdvSelect);
$ams1->setButtonAttributes('add', array_merge($multiSelectAdd, array('id' => 'AddEscalation')));
$ams1->setButtonAttributes('remove', array_merge($multiSelectRemove, array('id' => 'RemoveEscalation')));
$ams1->setElementTemplate($template);
echo $ams1->getElementJs(false);

/* Dependencies */
$bams = array();
$DBRESULT = $pearDB->query("SELECT ba_id, name FROM mod_bam WHERE ba_id != '".$id_ba."' ORDER BY name");
while($row = $DBRESULT->fetchRow()) {
	$bams[$row["ba_id"]] = htmlspecialchars($row["name"]);
}

$tab = array();
$tab[] = &HTML_QuickForm::createElement('checkbox', 'o', '&nbsp;', 'Ok');
$tab[] = &HTML_QuickForm::createElement('checkbox', 'w', '&nbsp;', 'Warning');
$tab[] = &HTML_QuickForm::createElement('checkbox', 'c', '&nbsp;', 'Critical');
$tab[] = &HTML_QuickForm::createElement('checkbox', 'n', '&nbsp;', 'None');
$form->addGroup($tab, 'notification_failure_criteria', _("Notification failure criteria"), '&nbsp;&nbsp;');

$tab = array();
$tab[] = &HTML_QuickForm::createElement('checkbox', 'o', '&nbsp;', 'Ok');
$tab[] = &HTML_QuickForm::createElement('checkbox', 'w', '&nbsp;', 'Warning');
$tab[] = &HTML_QuickForm::createElement('checkbox', 'c', '&nbsp;', 'Critical');
$tab[] = &HTML_QuickForm::createElement('checkbox', 'n', '&nbsp;', 'None');
$form->addGroup($tab, 'execution_failure_criteria', _("Execution failure criteria"), '&nbsp;&nbsp;');

$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'inherits_parent', null, _("Yes"), '1');
$tab[] = &HTML_QuickForm::createElement('radio', 'inherits_parent', null, _("No"), '0');
$form->addGroup($tab, 'inherits_parent', _("Inherits from parents"), '&nbsp;');

/* BA Group List */
$ba_group_list = array();
$DBRESULT = $pearDB->query("SELECT id_ba_group, ba_group_name FROM mod_bam_ba_groups ORDER BY ba_group_name");
while($row = $DBRESULT->fetchRow()) {
	$ba_group_list[$row["id_ba_group"]] = htmlspecialchars($row["ba_group_name"]);
}
$ams1 = $form->addElement('advmultiselect', 'ba_group_list', _("Linked Business Activity Groups"), $ba_group_list, $attrsAdvSelect);
$ams1->setButtonAttributes('add', $multiSelectAdd);
$ams1->setButtonAttributes('remove', $multiSelectRemove);
$ams1->setElementTemplate($template);
echo $ams1->getElementJs(false);

/* Event Handlers */ 
$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'event_handler_enabled', null, _("Yes"), '1');
$tab[] = &HTML_QuickForm::createElement('radio', 'event_handler_enabled', null, _("No"), '0');
$form->addGroup($tab, 'event_handler_enabled', _("Enable Event Handler"), '&nbsp;');
$form->setDefaults(array('event_handler_enabled' => '0'));

/*
 * Commands
 */
$attrCommand1 = array_merge(
    $attrCommands,
    array(
        'defaultDatasetRoute' => './include/common/webServices/rest/internal.php?object=centreon_configuration_command&action=defaultValues&target=service&field=command_command_id&id=' . $service_id,
        'availableDatasetRoute' => './include/common/webServices/rest/internal.php?object=centreon_configuration_command&action=list&t=3'
    )
);
$EHCommandSelect = $form->addElement('select2', 'event_handler_command', _("Event Handler Command"), array(), $attrCommand1);
$EHCommandSelect->addJsCallback('change', 'setArgument(jQuery(this).closest("form").get(0),"event_handler_command","example2");');

$form->addElement('text', 'event_handler_args', _("Args"), $attrsText);

/**
 * Extended information
 */

/* Visual settings */

// Icons
$imgList = array(NULL => NULL);
$query = "SELECT img_id, img_name, img_path, dir_name " .
		"FROM view_img_dir, view_img, view_img_dir_relation vidr " .
		"WHERE img_id = vidr.img_img_id " .
		"AND dir_id = vidr.dir_dir_parent_id " .
		"ORDER BY dir_name, img_name";
$DBRESULT = $pearDB->query($query);
$is_a_valid_image = array('gif'=>'gif', 'png'=>'png', 'jpg'=>'jpg');
$dir_name = NULL;
$dir_name2 = NULL;
$cpt = 1;
while ($elem = $DBRESULT->fetchRow()) {
	$dir_name = $elem["dir_name"];
	if ($dir_name2 != $dir_name) {
		$dir_name2 = $dir_name;
		$imgList["REP_".$cpt] = $dir_name;
		$cpt++;
	}
	$ext = NULL;
	$pinfo = pathinfo($elem["img_path"]);
	if (isset($pinfo["extension"]) && isset($is_a_valid_image[$pinfo["extension"]])) {
		$ext = "&nbsp;(".$pinfo["extension"].")";
	}
	$imgList[$elem["img_id"]] = "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;".html_entity_decode($elem["img_name"], ENT_NOQUOTES).$ext;
}
$form->addElement('select', 'icon', _("Icon"), $imgList);

/************************/

/* Centreon BI settings */

// Reporting time periods
$ams2 = $form->addElement('advmultiselect', 'reporting_timeperiods', _("Extra reporting time periods used in Centreon BI reports"), $tab_timeperiod, $attrsAdvSelect);
$ams2->setButtonAttributes('add', $multiSelectAdd);
$ams2->setButtonAttributes('remove', $multiSelectRemove);
$ams2->setElementTemplate($template);
echo $ams2->getElementJs(false);

// SLA
$form->addElement('text', 'sla_month_percent_warn', _("SLA warning percentage threshold"), $attrsTextSmall);
$form->addElement('text', 'sla_month_percent_crit', _("SLA critical percentage threshold"), $attrsTextSmall);
$form->addElement('text', 'sla_month_duration_warn', _("SLA warning duration threshold"), $attrsTextSmall);
$form->addElement('text', 'sla_month_duration_crit', _("SLA critical duration threshold"), $attrsTextSmall);

/************************/

$ba_id = $form->addElement('hidden', 'ba_id');
$redirect = $form->addElement('hidden', 'o');
$redirect->setValue($o);

/* Rules */
$form->applyFilter('__ALL__', 'myTrim');
$form->registerRule('exist', 'callback', 'testBAExistence');
$form->registerRule('checkThreshold', 'callback', 'checkThresholds');
$form->registerRule('checkPercentage', 'callback', 'checkPercentage');
$form->registerRule('checkInstanceService', 'callback', 'checkInstanceService');

$form->addRule('ba_name', _("Compulsory Name"), 'required');
$form->addRule('ba_desc', _("Compulsory Description"), 'required');
$form->addRule('ba_name', _("Name already in use"), 'exist');
$form->addRule('ba_warning', _("Compulsory Field"), 'required');
$form->addRule('ba_critical', _("Compulsory Field"), 'required');
$form->addRule('bam_contact', _("Compulsory Field"), 'required');
$form->addRule('notif_interval', _("Compulsory Field"), 'required');
$form->addRule('additional_poller', _("The service and BA are not linked in the same poller"), 'checkInstanceService');

$form->addRule('ba_warning', _("Warning Threshold must be greater than Critical Threshold (0-100)"), 'checkThreshold');
$form->addRule('ba_critical', _("Critical Threshold must be less than Warning Threshold (0-100)"), 'checkThreshold');

$form->addRule('sla_month_percent_warn', _('Must be between 0 and 100'), 'checkPercentage');
$form->addRule('sla_month_percent_crit', _('Must be between 0 and 100'), 'checkPercentage');

$form->setRequiredNote("<font style='color: red;'>*</font>". _(" Required fields"));

if ($o == "a") {
	$subS = $form->addElement('submit', 'submitSaveAdd', _("Save"), $attrBtnSuccess);
} elseif($o == "c") {
	$subS = $form->addElement('submit', 'submitSaveChange', _("Save"), $attrBtnSuccess);
}
$form->setDefaults($data_tab);

/*
 *  Smarty template
 */
$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl);
$tpl->assign('o', $o);
$tpl->assign('p', $p);

/* Images */
$tpl->assign("img_tool", "./modules/centreon-bam-server/core/common/images/tool.gif");
$tpl->assign("img_info", "./modules/centreon-bam-server/core/common/images/about.png");
$tpl->assign("img_escalation", "./modules/centreon-bam-server/core/common/images/step.png");
$tpl->assign("img_timeperiod", "./modules/centreon-bam-server/core/common/images/clock.png");
$tpl->assign("img_notif", "./modules/centreon-bam-server/core/common/images/mail_write.gif");
$tpl->assign("img_settings", "./modules/centreon-bam-server/core/common/images/wrench.png");
$tpl->assign("img_ba_list", "./modules/centreon-bam-server/core/common/images/gear.gif");
$tpl->assign("img_sla", "./modules/centreon-bam-server/core/common/images/reporting.gif");

/* Labels */
$tpl->assign("escal_title", _("Escalations"));
$tpl->assign("timeperiod_title", _("Reporting time periods"));
$tpl->assign("notif_title", _("Notification"));
$tpl->assign("sla_title", _("Service Level Agreement"));
$tpl->assign("settings_title", _("Settings"));
$tpl->assign("ba_list_title", _("Business Activity Group"));
$tpl->assign("graphic_label", _("Visual settings"));
$tpl->assign('time_unit', " * ".$oreon->Nagioscfg["interval_length"]." "._("seconds"));

/* Labels for BI */
$tpl->assign('centreon_bi_settings', _('Centreon BI settings'));
$tpl->assign('sla_unit_percent', '(0 - 100%)');
$tpl->assign('sla_unit_minutes', _('minutes'));

$helptext = "";

include_once("help.php");

foreach ($help as $key => $text) {
    $helptext .= '<span style="display:none" id="help:'.$key.'">'.$text.'</span>'."\n";
}
$tpl->assign("helptext", $helptext);

require_once './include/configuration/configObject/service/javascript/argumentJs.php';

$valid = false;
if ($form->validate())	{
	$ba = new CentreonBam_Ba($pearDB, $form, $pearDBndo);
	$baObj = $form->getElement('ba_id');
	if ($form->getSubmitValue("submitSaveAdd")) {
		$newBaId = $ba->insertBAInDB();
		$baObj->setValue($newBaId);
		getCentreonBaServiceId($pearDB, 'ba_' . $newBaId);
	} else if ($form->getSubmitValue("submitSaveChange")) {
		$ba->updateBA($baObj->getValue());
	}
	$o = NULL;
	$form->addElement("button", "change", _("Modify"), array("onClick"=>"javascript:window.location.href='?p=".$p."&o=c&ba_id=".$baObj->getValue()."'"));
	$form->freeze();
	$valid = true;
}

if ($valid) {
	require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/ba/configuration_ba_list.php");
} else {
	$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
	$renderer->setRequiredTemplate('{$label}&nbsp;<font color="red" size="1">*</font>');
	$renderer->setErrorTemplate('<font color="red">{$error}</font><br />{$html}');
	$form->accept($renderer);
	$tpl->assign('form', $renderer->toArray());
    $tpl->display("configuration_ba_form.ihtml");
}
?>
<script type='text/javascript' src='./modules/centreon-bam-server/core/common/javascript/initHelpTooltips.js'></script>
<script type='text/javascript' src='./modules/centreon-bam-server/core/configuration/ba/javascript/reportingTimePeriods.js'></script>
