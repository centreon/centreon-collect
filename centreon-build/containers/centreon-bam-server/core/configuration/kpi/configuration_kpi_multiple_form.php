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
require_once ("./modules/centreon-bam-server/core/configuration/kpi/rulesFunctions.php");
require_once ("./modules/centreon-bam-server/core/configuration/kpi/javascript/multiple_kpi_js.php");
$path = "./modules/centreon-bam-server/core/configuration/kpi/template";

$versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

$attrsText 		= array("size"=>"30");
$attrsText2 	= array("size"=>"60");
$attrsTextSmall = array("size"=>"5");
$attrsAdvSelect = array("style" => "width: 200px; height: 100px;", "id" => "ba_list_id");
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

$data_tab = array();
$data_tab['config_mode'] = '0';

/* Basic info */
$form = new HTML_QuickForm('Form', 'post', "?p=".$p."&o=".$o);
if ($o == "am") {
	$form->addElement('header', 'title', _("Key Performance Indicator"));
}

$form->addElement('header', 'information', _("Information"));

/* Configuration mode */
$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'config_mode', null, _("Regular"), '0', array('id'=>'config_mode_reg'));
$tab[] = &HTML_QuickForm::createElement('radio', 'config_mode', null, _("Advanced"), '1', array('id'=>'config_mode_adv'));
$form->addGroup($tab, 'config_mode', _("Configuration Mode"), '&nbsp;');


/* Object Type */
$tab_type = array(NULL=>NULL, "0"=>_("Host"), "1"=>_("Host Groups"), "2"=>_("Service Groups"));
$form->addElement('select', 'obj_type', _("Object Type"), $tab_type, array("id" => "obj_type", "onChange" =>"javascript:swap_object(this.value);"));

/*
 *  Load list of hosts
 */
$host_tab = array(NULL => NULL);
$query = "SELECT host_id, host_name FROM `host` WHERE host_register = '1' ORDER BY host_name";
$DBRES = $pearDB->query($query);
while ($row = $DBRES->fetchRow()) {
	$host_tab[$row['host_id']] = $row['host_name'];	
}
$form->addElement('select', 'host_list', _("Hosts"), $host_tab, array("id" => "host_list"));

/*
 *  Load list of host groups
 */
$hg_tab = array(NULL => NULL);
$query = "SELECT hg_id, hg_name FROM `hostgroup` ORDER BY hg_name";
$DBRES = $pearDB->query($query);
while ($row = $DBRES->fetchRow()) {
	$hg_tab[$row['hg_id']] = $row['hg_name'];	
}
$form->addElement('select', 'hg_list', _("Host Groups"), $hg_tab, array("id" => "hg_list"));

/*
 *  Load list of host groups
 */
$sg_tab = array(NULL => NULL);
$query = "SELECT sg_id, sg_name FROM `servicegroup` ORDER BY sg_name";
$DBRES = $pearDB->query($query);
while ($row = $DBRES->fetchRow()) {
	$sg_tab[$row['sg_id']] = $row['sg_name'];	
}
$form->addElement('select', 'sg_list', _("Service Groups"), $sg_tab, array("id" => "sg_list"));

/* Business Activity List */
$ba_list = array();
$DBRESULT = $pearDB->query("SELECT ba_id, name FROM `mod_bam`");
while($row = $DBRESULT->fetchRow()) {
	$ba_list[$row["ba_id"]] = htmlspecialchars($row["name"]);
}
$ams1 = $form->addElement('advmultiselect', 'ba_list', _("Linked Business Activity"), $ba_list, $attrsAdvSelect);
$ams1->setButtonAttributes('add', $multiSelectAdd);
$ams1->setButtonAttributes('remove', $multiSelectRemove);
$ams1->setElementTemplate($template);
echo $ams1->getElementJs(false);

$kpi_id = $form->addElement('hidden', 'kpi_id');
$redirect = $form->addElement('hidden', 'o');
$redirect->setValue($o);

/* Rules */
$form->applyFilter('__ALL__', 'myTrim');
$form->registerRule('exist', 'callback', 'testKPIExistence');
$form->registerRule('checkInfiniteLoops', 'callback', 'checkInfiniteLoop');
$form->registerRule('checkObjectInstances', 'callback', 'checkObjectInstance');
$form->addRule('ba_list', _("Compulsory Field"), 'required');
$form->addRule('ba_list', _("Circular definitions of Business Activities"), 'checkInfiniteLoops');
$form->addRule('ba_list', _("The service and BA are not linked in the same poller"), 'checkObjectInstances');

$form->setRequiredNote("<font style='color: red;'>*</font>". _(" Required fields"));

$subS = $form->addElement('button', 'submitSaveAdd', _("Retrieve KPI"), array_merge(array("onClick" => "loadKPIList();"), $attrBtnSuccess));


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
$tpl->assign("acl_title", _("Access Control List"));
$tpl->assign("ba_list_title", _("Business Activity List"));

$helptext = "";
include_once("help.php");
foreach ($help as $key => $text) {
    $helptext .= '<span style="display:none" id="help:'.$key.'">'.$text.'</span>'."\n";
}
$tpl->assign("helptext", $helptext);

$kpi_select = 0;
$svc_select = 0;
$valid = false;

if ($form->validate()) {
	$select = $_POST['select'];
	$kpiObj = new CentreonBam_Kpi($pearDB);
	foreach ($select as $key => $value) {
		$kpiObj->insertMultipleKPI($key, $form);
	}
	$o = null;
	$form->freeze();
	$valid = true;
}
if ($valid) {
	require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_list.php");
}
else {	
	$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
	$renderer->setRequiredTemplate('{$label}&nbsp;<font color="red" size="1">*</font>');
	$renderer->setErrorTemplate('<font color="red">{$error}</font><br />{$html}');
	$form->accept($renderer);
	$tpl->assign('form', $renderer->toArray());
    
    $tpl->display("configuration_kpi_multiple_form.ihtml");
    
    ?>
        <script type='text/javascript' src='./modules/centreon-bam-server/core/common/javascript/initHelpTooltips.js'></script>
	<script type="text/javascript">initialize_select_list()</script>
<?php
}
?>
