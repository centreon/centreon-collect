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
require_once ("./modules/centreon-bam-server/core/configuration/group/rulesFunctions.php");

$path = "./modules/centreon-bam-server/core/configuration/group/template";

$versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

if ($o == "c" && isset($id_ba_group)) {
	$query = "SELECT * FROM `mod_bam_ba_groups` WHERE id_ba_group = '".$id_ba_group."'";
	$DBRES = $pearDB->query($query);
	$data_tab = array();

	# SET INFO
	while ($row = $DBRES->fetchRow()) {
		$data_tab['ba_group_name'] = htmlspecialchars($row['ba_group_name']);
		$data_tab['ba_group_desc'] = htmlspecialchars($row['ba_group_description']);
		$data_tab['ba_group_display'] = $row['visible'];
		$data_tab['ba_group_id'] = $row['id_ba_group'];
	}

	# SET BA LIST
	$query = "SELECT * FROM `mod_bam_bagroup_ba_relation` WHERE id_ba_group = '".$id_ba_group."'";
	$DBRES = $pearDB->query($query);
	while ($row = $DBRES->fetchRow()) {
		$data_tab['ba_list'][] = $row['id_ba'];
	}

	# SET ACL
	$DBRESULT = $pearDB->query("SELECT acl_group_id FROM `mod_bam_acl` WHERE ba_group_id = '".$id_ba_group."'");
	while($row = $DBRESULT->fetchRow()) {
		$data_tab["bam_acl"][] = $row["acl_group_id"];
	}
}

$attrsText 		= array("size"=>"30");
$attrsText2 	= array("size"=>"60");
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

/*Basic info */
$form = new HTML_QuickForm('Form', 'post', "?p=".$p);
$form->addElement('header', 'title', _("Business Activity View"));
$form->addElement('header', 'information', _("Information"));
$form->addElement('text', 'ba_group_name', _("Name"), $attrsText);
$form->addElement('text', 'ba_group_desc', _("Description"), $attrsText);

/* Displayed yes/no */
$ba_group_display[] = &HTML_QuickForm::createElement('radio', 'ba_group_display', null, _("Yes"), '1');
$ba_group_display[] = &HTML_QuickForm::createElement('radio', 'ba_group_display', null, _("No"), '0');
$form->addGroup($ba_group_display, 'ba_group_display', _("Displayed in Overview"), '&nbsp;');
if ($o == "a") {
	$form->setDefaults(array('ba_group_display' => '1'));
}

/* Business Activity List */
$ba_list = array();
$DBRESULT = $pearDB->query("SELECT ba_id, name FROM `mod_bam`");
while($row = $DBRESULT->fetchRow())
	$ba_list[$row["ba_id"]] = htmlspecialchars($row["name"]);
$ams1 = $form->addElement('advmultiselect', 'ba_list', _("Linked Business Activity"), $ba_list, $attrsAdvSelect);
$ams1->setButtonAttributes('add', $multiSelectAdd);
$ams1->setButtonAttributes('remove', $multiSelectRemove);
$ams1->setElementTemplate($template);
echo $ams1->getElementJs(false);


/* ACL */
$acls = array();
$DBRESULT = $pearDB->query("SELECT acl_group_id, acl_group_name FROM `acl_groups`");
while($row = $DBRESULT->fetchRow())
	$acls[$row["acl_group_id"]] = htmlspecialchars($row["acl_group_name"]);
$ams1 = $form->addElement('advmultiselect', 'bam_acl', _("Authorized Access Groups"), $acls, $attrsAdvSelect);
$ams1->setButtonAttributes('add', $multiSelectAdd);
$ams1->setButtonAttributes('remove', $multiSelectRemove);
$ams1->setElementTemplate($template);
echo $ams1->getElementJs(false);

$ba_g_id = $form->addElement('hidden', 'ba_group_id');
$redirect = $form->addElement('hidden', 'o');
$redirect->setValue($o);

/* Rules */
$form->applyFilter('__ALL__', 'myTrim');
$form->registerRule('exist', 'callback', 'testBAGroupExistence');
$form->addRule('ba_group_name', _("Compulsory Name"), 'required');
$form->addRule('ba_group_desc', _("Compulsory Description"), 'required');
$form->addRule('ba_group_name', _("Name already in use"), 'exist');
$form->setRequiredNote("<font style='color: red;'>*</font>". _(" Required fields"));

if ($o == "a") {
	$subS = $form->addElement('submit', 'submitSaveAdd', _("Save"), $attrBtnSuccess);
} else if($o == "c") {
	$subS = $form->addElement('submit', 'submitSaveChange', _("Save"), $attrBtnSuccess);
	$form->setDefaults($data_tab);
}

/*
 *  Smarty template
 */
$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl);
$tpl->assign('o', $o);
$tpl->assign('p', $p);
$tpl->assign("img_group", "./modules/centreon-bam-server/core/common/images/cube_green.gif");
$tpl->assign("img_info", "./modules/centreon-bam-server/core/common/images/about.png");
$tpl->assign("img_acl", "./modules/centreon-bam-server/core/common/images/user1_lock.png");
$tpl->assign("img_ba_list", "./modules/centreon-bam-server/core/common/images/gear.gif");
$tpl->assign("acl_title", _("Access Control List"));
$tpl->assign("ba_list_title", _("Business Activity List"));

$valid = false;
if ($form->validate())	{
	$baGroup = new CentreonBam_BaGroup($pearDB, $form);
	$baObj = $form->getElement('ba_group_id');
	if ($form->getSubmitValue("submitSaveAdd")) {
		$baObj->setValue($baGroup->insertBAGroupInDB());
	}
	elseif ($form->getSubmitValue("submitSaveChange")) {
		$baGroup->updateBAGroup($baObj->getValue());
	}
	$o = NULL;
	$form->addElement("button", "change", _("Modify"), array("onClick"=>"javascript:window.location.href='?p=".$p."&o=c&ba_group_id=".$baObj->getValue()."'"));
	$form->freeze();
	$valid = true;
}

if ($valid) {
	require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/group/configuration_ba_group_list.php");
}
else {	
	$helptext = "";
	include_once("help.php");
	foreach ($help as $key => $text) {
		$helptext .= '<span style="display:none" id="help:'.$key.'">'.$text.'</span>'."\n";
	}
	$tpl->assign("helptext", $helptext);
    
        $renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
	$renderer->setRequiredTemplate('{$label}&nbsp;<font color="red" size="1">*</font>');
	$renderer->setErrorTemplate('<font color="red">{$error}</font><br />{$html}');
	$form->accept($renderer);
	$tpl->assign('form', $renderer->toArray());
    $tpl->display("configuration_ba_group_form.ihtml");
}
?>
<script type='text/javascript' src='./modules/centreon-bam-server/core/common/javascript/initHelpTooltips.js'></script>
