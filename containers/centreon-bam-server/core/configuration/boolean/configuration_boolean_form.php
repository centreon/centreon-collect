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
require_once ("./modules/centreon-bam-server/core/configuration/boolean/rulesFunctions.php");
require_once ("./modules/centreon-bam-server/core/configuration/boolean/javascript/boolean_js.php");

$path = "./modules/centreon-bam-server/core/configuration/boolean/template";

$versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

$utils = new CentreonBam_Utils($pearDB);
$bool = new CentreonBam_Boolean($pearDB, new CentreonBam_Db('centstorage'));
$data_tab = array();
$data_tab['config_type'] = '0';
$data_tab['activate'] = '1';
$data_tab['bool_state'] = '1';
if ($o == "c" && isset($boolean_id)) {
    $query = "SELECT * FROM `mod_bam_boolean` WHERE boolean_id = ".$pearDB->escape($boolean_id);
    $DBRESULT = $pearDB->query($query);
    $row = $DBRESULT->fetchRow();
    $data_tab = $row;
    $data_tab['activate'] = $row['activate'];
}

$attrsText 	= array("size" => "30");
$attrsText2 	= array("size" => "60");
$attrsTextSmall = array("size" => "5");
$attrsAdvSelect = array("style" => "width: 200px; height: 100px;");
$attrsTextarea 	= array("rows" => "9", "cols"=>"65");
$template 	= "<table><tr><td>{unselected}</td><td align='center'>{add}<br><br><br>{remove}</td><td>{selected}</td></tr></table>";
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

/* Basic info */
$form = new HTML_QuickForm('Form', 'post', "?p=".$p);
$form->addElement('header', 'title', _("Boolean KPI"));
$form->addElement('header', 'information', _("Information"));

$form->addElement('text', 'name', _("Boolean name"), $attrsText);

/* Configuration mode */
$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'config_type', null, _("Regular"), '0', array('id'=>'config_mode_reg', 'onClick'=>'impactTypeProcessBool();'));
$tab[] = &HTML_QuickForm::createElement('radio', 'config_type', null, _("Advanced"), '1', array('id'=>'config_mode_adv', 'onClick'=>'impactTypeProcessBool();'));
$form->addGroup($tab, 'config_type', _("Configuration Mode"), '&nbsp;');

$form->addElement('textarea', 'expression', _('Expression'), array("rows" => "9", "cols"=>"65", "id"=>"boolExp", "onClick" => "hideSimul();"));

$form->addElement('select', 'resource_host', NULL, array_merge(array(''=>_('Select host')),
                                                               $hst->getHostNames()), 
                                                   array('id' => 'resource_host', 'onchange'=>'loadServiceListBool(this.value);'));
$form->addElement('select', 'resource_service', NULL, $svc->getServiceNames(), array('id' => 'resource_service'));

$form->addElement('select', 'exp_operator', NULL, array('IS' => 'is', 'NOT' => 'is not'), array('id' => 'exp_operator'));

$states = array('OK' => 'Ok', 'WARNING' => 'Warning', 'CRITICAL' => 'Critical', 'UNKNOWN' => 'Unknown');
$form->addElement('select', 'resource_states', NULL, $states, array('id' => 'resource_states'));


/* Boolean state */
$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'bool_state', null, _("True"), '1');
$tab[] = &HTML_QuickForm::createElement('radio', 'bool_state', null, _("False"), '0');
$form->addGroup($tab, 'bool_state', _("Impact is applied when expression returns:"), '&nbsp;');

$form->addElement('textarea', 'comments', _('Comments'), $attrsTextarea);

$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'activate', null, _("Disabled"), '0');
$tab[] = &HTML_QuickForm::createElement('radio', 'activate', null, _("Enabled"), '1');
$form->addGroup($tab, 'activate', _("Status"), '&nbsp;');

/* Rules */
$compulsoryTxt = _('Compulsory field');
$form->applyFilter('__ALL__', 'myTrim');
$form->registerRule('exist', 'callback', 'testBoolExistence');
$form->registerRule('evalexp', 'callback', 'testExpValidity');
$form->addRule('name', $compulsoryTxt, 'required');
$form->addRule('name', _('Name already in use'), 'exist');
$form->addRule('expression', $compulsoryTxt, 'required');
$form->addRule('expression', _('Invalid Expression'), 'evalexp');
$form->setRequiredNote("<font style='color: red;'>*</font>". _(" Required fields"));

if ($o == "a") {
    $subS = $form->addElement('submit', 'submitSaveAdd', _("Save"), $attrBtnSuccess);
} elseif($o == "c") {
    $subS = $form->addElement('submit', 'submitSaveChange', _("Save"), $attrBtnSuccess);
}
$form->addElement('hidden', 'boolean_id');
$redirect = $form->addElement('hidden', 'o');
$redirect->setValue($o);
$form->setDefaults($data_tab);

/*
 *  Smarty template
 */
$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl);
$tpl->assign('o', $o);
$tpl->assign('p', $p);
$tpl->assign("img_group", "./modules/centreon-bam-server/core/common/images/cube_green.gif");
$tpl->assign("img_info", "./modules/centreon-bam-server/core/common/images/about.png");
$tpl->assign("img_kpi", "./modules/centreon-bam-server/core/common/images/gear.gif");
$tpl->assign("ba_list_title", _("Business Activity List"));

$helptext = "";
include_once("help.php");
foreach ($help as $key => $text) {
    $helptext .= '<span style="display:none" id="help:'.$key.'">'.$text.'</span>'."\n";
}
$tpl->assign("helptext", $helptext);

$valid = false;
if ($form->validate())	{
    $boolObj = $form->getElement('boolean_id');
    if ($form->getSubmitValue("submitSaveAdd")) {
        $boolObj->setValue($bool->insert($form->getSubmitvalues()));
    } elseif ($form->getSubmitValue("submitSaveChange")) {
        $bool->update($boolObj->getValue(), $form->getSubmitValues());
    }
    $o = null;
    $form->addElement("button", "change", _("Modify"), array("onClick"=>"javascript:window.location.href='?p=".$p."&o=bc&boolId=".$boolObj->getValue()."'"));
    $form->freeze();
    $valid = true;
}

if ($valid) {
    require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/boolean/configuration_boolean_list.php");
} else {
    $renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
    $renderer->setRequiredTemplate('{$label}&nbsp;<font color="red" size="1">*</font>');
    $renderer->setErrorTemplate('<font color="red">{$error}</font><br />{$html}');
    $form->accept($renderer);
    $tpl->assign('applyLabel', _(' < < '));
    $tpl->assign('resultLabel', _('Real time result'));
    $tpl->assign('form', $renderer->toArray());

    $tpl->display("configuration_boolean_form.ihtml");
}
?>
<script type='text/javascript' src='./modules/centreon-bam-server/core/common/javascript/initHelpTooltips.js'></script>
