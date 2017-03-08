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
require_once ("./modules/centreon-bam-server/core/configuration/kpi/javascript/kpi_js.php");
$path = "./modules/centreon-bam-server/core/configuration/kpi/template";

$versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

$kpi_select = null;
$svc_select = null;
$kpi_type = null;
$data_tab = array();
$data_tab['config_mode'] = '0';

// Get infos in DB
if ($o == "c" && isset($kpi_id)) {
	$query = "SELECT * FROM `mod_bam_kpi` WHERE kpi_id = '".$kpi_id."' LIMIT 1";
	$DBRESULT = $pearDB->query($query);
	$row = $DBRESULT->fetchRow();
    
	$data_tab['kpi_type'] = $row['kpi_type'];
	$data_tab['kpi_id'] = $kpi_id;
	$data_tab['ba_list'][] = $row['id_ba'];

    // Advanced
	$data_tab['unknown_impact'] = $row['drop_unknown'];
	$data_tab['warning_impact'] = $row['drop_warning'];
	$data_tab['critical_impact'] = $row['drop_critical'];
    if ($row['kpi_type'] == 3) {
        $data_tab["boolean_impact"] = $row['drop_critical'];
        $data_tab['critical_impact'] = '';
    } else {
        $data_tab['critical_impact'] = $row['drop_critical'];
    }

    // Regular
    $data_tab['unknown_impact_regular'] = $row['drop_unknown_impact_id'];
	$data_tab['warning_impact_regular'] = $row['drop_warning_impact_id'];
    if ($row['kpi_type'] == 3) {
        $data_tab['boolean_impact_regular'] = $row['drop_critical_impact_id'];
        $data_tab['critical_impact_regular'] = null;
    } else {
        $data_tab['critical_impact_regular'] = $row['drop_critical_impact_id'];
	}

    $data_tab['config_mode'] = $row['config_type'];

	$ba_name = $ba->getBA_Name($row['id_ba']);
	$svc_selected = NULL;
	switch ($row['kpi_type']) {
		case "0" : $kpi_select = $row['host_id']; $svc_select = $row['service_id'];break;
		case "1" : $kpi_select = $row['meta_id']; break;
		case "2" : $kpi_select = $row['id_indicator_ba']; break;
		case "3" : $kpi_select = $row['boolean_id']; break;
	}
	$kpi_type = $row['kpi_type'];
} else {
	$options = new CentreonBam_Options($pearDB, $oreon->user->user_id);
	$opt = $options->getGeneralOptions();

	$data_tab['warning_impact'] = $opt['kpi_warning_impact'];
	$data_tab['critical_impact'] = $opt['kpi_critical_impact'];
	$data_tab['unknown_impact'] = $opt['kpi_unknown_impact'];
}

if (isset($_POST['kpi_select'])) {
	$kpi_select = $_POST['kpi_select'];
}
if (isset($_POST['svc_select'])) {
	$svc_select = $_POST['svc_select'];
}
if (isset($_POST['kpi_type'])) {
	$kpi_type = $_POST['kpi_type'];
}

$attrsText 		= array("size" => "30");
$attrsText2 	= array("size" => "60");
$attrsTextSmall = array("size" => "5");
$attrsAdvSelect = array("style" => "width: 200px; height: 100px;");
$attrsTextarea 	= array("rows" => "5", "cols"=>"40");
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

/* Basic info */
$form = new HTML_QuickForm('Form', 'post', "?p=".$p);
$form->addElement('header', 'title', _("Key Performance Indicator"));
$form->addElement('header', 'information', _("Information"));
$form->addElement('header', 'impact', _("Impact levels"));
$form->addElement('header', 'impact_ba', _("Business Activities"));

//-----------------------------------------------------
// Boolean expression
$form->addElement('text', 'boolean_impact', _("Boolean Business Impact"), $attrsTextSmall);

// Regular expression
$form->addElement('text', 'warning_impact', _("Warning Business Impact"), $attrsTextSmall);
$form->addElement('text', 'critical_impact', _("Critical Business Impact"), $attrsTextSmall);
$form->addElement('text', 'unknown_impact', _("Unknown Business Impact"), $attrsTextSmall);

//-----------------------------------------------------
// Boolean expression
$boolImpact = $form->addElement('select', 'boolean_impact_regular', _("Boolean Business Impact"), array(), array('id' => 'boolean_impact_regular'));

// Regular expression
$warnImpact = $form->addElement('select', 'warning_impact_regular', _("Warning Business Impact"), array(), array('id' => 'warning_impact_regular'));
$critImpact = $form->addElement('select', 'critical_impact_regular', _("Critical Business Impact"), array(), array('id' => 'critical_impact_regular'));
$unknImpact = $form->addElement('select', 'unknown_impact_regular', _("Unknown Business Impact"), array(), array('id' => 'unknown_impact_regular'));

// Inject Data
$kpiObj = new CentreonBam_Kpi($pearDB);
try {
    $kpiObj->buildImpactList($pearDB, $warnImpact);
    $kpiObj->buildImpactList($pearDB, $critImpact);
    $kpiObj->buildImpactList($pearDB, $unknImpact);
    $kpiObj->buildImpactList($pearDB, $boolImpact);
} catch (Exception $e) {
    echo $e->getMessage();
}

/* Configuration mode */
$tab = array();
$tab[] = &HTML_QuickForm::createElement('radio', 'config_mode', null, _("Regular"), '0', array('id'=>'config_mode_reg', 'onClick'=>'impactTypeProcess();'));
$tab[] = &HTML_QuickForm::createElement('radio', 'config_mode', null, _("Advanced"), '1', array('id'=>'config_mode_adv', 'onClick'=>'impactTypeProcess();'));
$form->addGroup($tab, 'config_mode', _("Configuration Mode"), '&nbsp;');

/* KPI Type */
$tab_type = array(
	null => null,
	"0" => _("Regular Service"),
	"1" => _("Meta Service"),
	"2" => _("Business Activity"),
	"3" => _("Boolean rule")
);

$form->addElement(
    'select', 
	'kpi_type', 
	_("KPI Type"), 
	$tab_type, 
	array("id" => "kpi_type", "onChange" =>"javascript:loadIndicatorList(this.value, 0);impactTypeProcess();")
);

/* Business Activity List */
$ba_list = array();
$DBRESULT = $pearDB->query("SELECT ba_id, name FROM `mod_bam`");
while ($row = $DBRESULT->fetchRow()) {
	$ba_list[$row["ba_id"]] = htmlspecialchars($row["name"]);
}

$ams1 = $form->addElement('advmultiselect', 'ba_list', _("Linked Business Activity"), $ba_list, $attrsAdvSelect);
$ams1->setButtonAttributes('add', $multiSelectAdd);
$ams1->setButtonAttributes('remove', $multiSelectRemove);
$ams1->setElementTemplate($template);
echo $ams1->getElementJs(false);

// Add hidden fields
$kpi_id = $form->addElement('hidden', 'kpi_id');
if (isset($_POST['kpi_select']) && $_POST['kpi_select']) {
    $kpi_id->setValue($_POST['kpi_select']);
}
$redirect = $form->addElement('hidden', 'o');
$redirect->setValue($o);

/* Rules */
$form->applyFilter('__ALL__', 'myTrim');
$form->registerRule('exist', 'callback', 'testKPIExistence');
$form->registerRule('isEnabled', 'callback', 'testKPIConfigurationStatus');
$form->registerRule('checkInfiniteLoops', 'callback', 'checkInfiniteLoop');
$form->registerRule('checkValue', 'callback', 'checkSelectedValues');
$form->registerRule('checkSelfBaAsKpi', 'callback', 'checkSelfBaAsKpi');
$form->registerRule('checkInstanceBa', 'callback', 'checkInstanceBa');

$form->addRule('ba_list', _("Compulsory Field"), 'required');
$form->addRule('kpi_type', _("Compulsory Field"), 'required');
$form->addRule('kpi_type', _("KPI already linked to one of the Business Activities"), 'exist');
$form->addRule('kpi_type', _("Service is disabled"), 'isEnabled');
$form->addRule('kpi_type', _("Compulsory Field"), 'checkValue');
$form->addRule('ba_list', _("Circular definitions of Business Activities"), 'checkInfiniteLoops');
$form->addRule('ba_list', _("Cannot add itself as KPI"), "checkSelfBaAsKpi");
$form->addRule('ba_list', _("The service and BA are not linked in the same poller"), "checkInstanceBa");
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

$valid = false;
if ($form->validate())	{
    $kpi = new CentreonBam_Kpi($pearDB, $form);
	$kpiObj = $form->getElement('kpi_id');
	if ($form->getSubmitValue("submitSaveAdd")) {
	    $kpiObj->setValue($kpi->insertKPIInDB());
	} elseif ($form->getSubmitValue("submitSaveChange")) {
	    $kpi->updateKPI($kpiObj->getValue());
	}
	$o = null;
	$form->addElement("button", "change", _("Modify"), array("onClick"=>"javascript:window.location.href='?p=".$p."&o=c&kpi_id=".$kpiObj->getValue()."'"));
	$form->freeze();
	$valid = true;
}

if ($valid) {
	require_once($centreon_path."www/modules/centreon-bam-server/core/configuration/kpi/configuration_kpi_list.php");
} else {
	$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
	$renderer->setRequiredTemplate('{$label}&nbsp;<font color="red" size="1">*</font>');
	$renderer->setErrorTemplate('<font color="red">{$error}</font><br />{$html}');
	$form->accept($renderer);
	$tpl->assign('form', $renderer->toArray());


    $tpl->display("configuration_kpi_form.ihtml");
?>
    <script type='text/javascript' src='./modules/centreon-bam-server/core/common/javascript/initHelpTooltips.js'></script>
<?php
}

if (isset($kpi_type) && !$valid) {
	?>
	<script type="text/javascript">
     // Load indicators list
	loadIndicatorList(<?php echo $kpi_type;?>, <?php echo $kpi_select;?>);
	</script>
	<?php
	if (isset($svc_select)) {
	?>
	<script type="text/javascript">
		loadServiceList(<?php echo $kpi_select;?>, <?php echo $svc_select?>);
    </script>
	<?php
	}
}
?>
    <script type='text/javascript'>
    // Hide Fields
    impactTypeProcess();
    </script>
