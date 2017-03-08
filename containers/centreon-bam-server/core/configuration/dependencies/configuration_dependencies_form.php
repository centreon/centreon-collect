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

$path = "./modules/centreon-bam-server/core/configuration/dependencies/template";

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

$form = new HTML_QuickForm('Form', 'post', "?p=" . $p . "&o=$o");
if ($o == "a") {
    $form->addElement('header', 'title', _("Add a Dependency"));
} elseif ($o == "c") {
    $form->addElement('header', 'title', _("Modify a Dependency"));
} elseif ($o == "w") {
    $form->addElement('header', 'title', _("View a Dependency"));
}

/*
 * Dependency basic information
 */
$form->addElement('header', 'information', _("Information"));
$form->addElement('text', 'dep_name', _("Name"), $attrsText);
$form->addElement('text', 'dep_description', _("Description"), $attrsText);

/* Dependencies */
$bams = array();
$DBRESULT = $pearDB->query("SELECT ba_id, name FROM mod_bam WHERE ba_id != '".$id_ba."' ORDER BY name");
while($row = $DBRESULT->fetchRow()) {
	$bams[$row["ba_id"]] = htmlspecialchars($row["name"]);
}

$tab = array();
$tab[] = &HTML_QuickForm::createElement('checkbox', 'o', '&nbsp;', 'Ok', array('id' => 'sOk', 'onClick' => 'uncheckAllS(this);'));
$tab[] = &HTML_QuickForm::createElement('checkbox', 'w', '&nbsp;', 'Warning', array('id' => 'sWarning', 'onClick' => 'uncheckAllS(this);'));
$tab[] = &HTML_QuickForm::createElement('checkbox', 'c', '&nbsp;', 'Critical', array('id' => 'sCritical', 'onClick' => 'uncheckAllSthis);'));
$tab[] = &HTML_QuickForm::createElement('checkbox', 'n', '&nbsp;', 'None', array('id' => 'sNone', 'onClick' => 'uncheckAllS(this);'));
$form->addGroup($tab, 'execution_failure_criteria', _("Execution failure criteria"), '&nbsp;&nbsp;');

$tab = array();
$tab[] = &HTML_QuickForm::createElement('checkbox', 'o', '&nbsp;', 'Ok', array('id' => 'sOk2', 'onClick' => 'uncheckAllS2(this);'));
$tab[] = &HTML_QuickForm::createElement('checkbox', 'w', '&nbsp;', 'Warning', array('id' => 'sWarning2', 'onClick' => 'uncheckAllS2(this);'));
$tab[] = &HTML_QuickForm::createElement('checkbox', 'c', '&nbsp;', 'Critical', array('id' => 'sCritical2', 'onClick' => 'uncheckAllS2(this);'));
$tab[] = &HTML_QuickForm::createElement('checkbox', 'n', '&nbsp;', 'None', array('id' => 'sNone2', 'onClick' => 'uncheckAllS2(this);'));
$form->addGroup($tab, 'notification_failure_criteria', _("Notification failure criteria"), '&nbsp;&nbsp;');

$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'inherits_parent', null, _("Yes"), '1');
$tab[] = &HTML_QuickForm::createElement('radio', 'inherits_parent', null, _("No"), '0');
$form->addGroup($tab, 'inherits_parent', _("Inherits from parents"), '&nbsp;');

//Parents
$ams1 = $form->addElement('advmultiselect', 'dep_bamParents', _("Parent Business Activity"), $bams, $attrsAdvSelect);
$ams1->setButtonAttributes('add', array_merge($multiSelectAdd, array('id' => 'AddParent')));
$ams1->setButtonAttributes('remove', $multiSelectRemove);
$ams1->setElementTemplate($template);
echo $ams1->getElementJs(false);

//Children
$ams1 = $form->addElement('advmultiselect', 'dep_bamChilds', _("Child Business Activity"), $bams, $attrsAdvSelect);
$ams1->setButtonAttributes('add', array_merge($multiSelectAdd, array('id' => 'AddChild')));
$ams1->setButtonAttributes('remove', $multiSelectRemove);
$ams1->setElementTemplate($template);
echo $ams1->getElementJs(false);

$form->addElement('hidden', 'dep_id');


$form->registerRule('checkCircularDependency', 'callback', 'checkCircularDependencies');
$form->addRule('dep_bamChilds', _("Circular definition of dependencies"), 'checkCircularDependency');

if ($o == "a") {
	$subS = $form->addElement('submit', 'submitSaveAdd', _("Save"), $attrBtnSuccess);
} elseif ($o == "c") {
	$subS = $form->addElement('submit', 'submitSaveChange', _("Save"), $attrBtnSuccess);
}

// get Dependency info
$depId = filter_input(INPUT_GET, 'dep_id');
if (($depId !== false ) && (!is_null($depId))) {
    $currentDepedency = CentreonBam_Dependency::loadById($centreonDb, $centreonMonitoringDb, $depId, $baObj);
    $dependencyDatas = $currentDepedency->getAllParametersAsArray();
    $form->setDefaults($dependencyDatas);
}

/*
 *  Smarty template
 */
$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl);
$tpl->assign('o', $o);
$tpl->assign('p', $p);


$valid = false;
if ($form->validate())	{
    $submittedValues = $form->getSubmitValues();
    $dependencyParameters = array();
    $dependencyParameters['name'] = $submittedValues['dep_name'];
    $dependencyParameters['description'] = $submittedValues['dep_description'];
    $dependencyParameters['inherit'] = $submittedValues['inherits_parent']['inherits_parent'];
    $dependencyParameters['execution_failure_criteria'] = implode(',', array_keys($submittedValues['execution_failure_criteria']));
    $dependencyParameters['notification_failure_criteria'] = implode(',', array_keys($submittedValues['notification_failure_criteria']));
    $dependencyParameters['parents'] = $submittedValues['dep_bamParents'];
    $dependencyParameters['children'] = $submittedValues['dep_bamChilds'];
    
    if ($o == 'a') {
        $dependencyObj->create($dependencyParameters);
    } elseif ($o == 'c') {
        $dependencyObj->setId($submittedValues['dep_id']);
        $dependencyObj->update($dependencyParameters);
    }
	$valid = true;
}

if ($valid) {
	require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/dependencies/configuration_dependencies_list.php");
} else {
	$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
	$renderer->setRequiredTemplate('{$label}&nbsp;<font color="red" size="1">*</font>');
	$renderer->setErrorTemplate('<font color="red">{$error}</font><br />{$html}');
	$form->accept($renderer);
	$tpl->assign('form', $renderer->toArray());
    $tpl->display("configuration_dependencies_form.ihtml");
}