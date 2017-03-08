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

$path = "./modules/centreon-bam-server/core/options/user/template";

$versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

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
$data_tab = $ba_options->getUserOptions();
	
/* Basic info */
$form = new HTML_QuickForm('Form', 'post', "?p=".$p."&o=".$o);
$form->addElement('header', 'title', _("User Settings"));

/* Custom Overview */
$ba_list = array();
$query = "SELECT ba_id, name FROM `mod_bam` " . 
		$ba_acl->queryBuilder("WHERE", "ba_id", $ba_acl->getBaStr());
		"ORDER BY name";

$DBRESULT = $pearDB->query($query);

while($row = $DBRESULT->fetchRow())
	$ba_list[$row["ba_id"]] = htmlspecialchars($row["name"]);
$ams2 = $form->addElement('advmultiselect', 'overview', _("Business Activities"), $ba_list, $attrsAdvSelect);
$ams2->setButtonAttributes('add', $multiSelectAdd);
$ams2->setButtonAttributes('remove', $multiSelectRemove);
$ams2->setElementTemplate($template);
echo $ams2->getElementJs(false);

/* Default RRD Graph Style */
$tab_style2 = array(NULL => NULL);
$query = "SELECT * FROM `giv_graphs_template` ORDER BY name";
$DBRES = $pearDB->query($query);
while ($row = $DBRES->fetchRow())
	$tab_style2[$row['graph_id']] = $row['name'];
$form->addElement('select', 'rrd_graph_style', _("RRD Graph Template"), $tab_style2);

/* Alpha */
$form->addElement('text', 'value_alpha', _("Business Activity Level Opacity"), $attrsTextSmall);
$form->addElement('text', 'warning_alpha', _("Warning Opacity"), $attrsTextSmall);
$form->addElement('text', 'critical_alpha', _("Critical Opacity"), $attrsTextSmall);

/* Colors */
$TabColorNameAndLang = array("value_color"=>_("Business Activity Level Color"),
							"warning_color"=>_("Warning Color"),
							"critical_color"=>_("Critical Color"));

while (list($nameColor, $val) = each($TabColorNameAndLang))	{
	$nameLang = $val;
	isset($data_tab[$nameColor]) ? $codeColor = "#".$data_tab[$nameColor] : $codeColor = NULL;
	$title = _("Pick a color");
	$attrsText3 	= array("value"=>$codeColor,"size"=>"9","maxlength"=>"7");
	$form->addElement('text', $nameColor, $nameLang,  $attrsText3);
	
	$attrsText4 	= array("style"=>"width:50px; height:18px; background: ".$codeColor." url() left repeat-x 0px; border-color:".$codeColor.";");
	$attrsText5 	= array_merge(array("onclick"=>"popup_color_picker('$nameColor','$nameLang','$title');"), $attrBtnDefault);
	$form->addElement('button', $nameColor.'_color', "", $attrsText4);
	$form->addElement('button', $nameColor.'_modify', _("Modify"), $attrsText5);	
}

/* Misc */
// Legend
$display_legend[] = &HTML_QuickForm::createElement('radio', 'display_legend', null, _("Yes"), '1');
$display_legend[] = &HTML_QuickForm::createElement('radio', 'display_legend', null, _("No"), '0');
$form->addGroup($display_legend, 'display_legend', _("Display Caption"), '&nbsp;');
// Guide
$display_guide[] = &HTML_QuickForm::createElement('radio', 'display_guide', null, _("Yes"), '1');
$display_guide[] = &HTML_QuickForm::createElement('radio', 'display_guide', null, _("No"), '0');
$form->addGroup($display_guide, 'display_guide', _("Display Graph Guide"), '&nbsp;');


$subS = $form->addElement('submit', 'submitSaveChange', _("Save"), $attrBtnSuccess);
$form->setDefaults($data_tab);	

/*
 *  Smarty template
 */
$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl);
$tpl->assign('o', $o);
$tpl->assign('p', $p);
$tpl->assign('colorJS',"
	<script type='text/javascript'>
		function popup_color_picker(t,name,title)
		{
			var width = 400;
			var height = 300;
			window.open('./include/common/javascript/color_picker.php?n='+t+'&name='+name+'&title='+title, 'cp', 'resizable=no, location=no, width='
						+width+', height='+height+', menubar=no, status=yes, scrollbars=no, menubar=no');
		}
	</script>");
$tpl->assign("img_style", "./modules/centreon-bam-server/core/common/images/component_green.png");
$tpl->assign("img_color", "./modules/centreon-bam-server/core/common/images/component_yellow.png");
$tpl->assign("img_opt", "./modules/centreon-bam-server/core/common/images/businessman.png");
$tpl->assign("img_misc", "./modules/centreon-bam-server/core/common/images/star_yellow.gif");
$tpl->assign("img_overview", "./modules/centreon-bam-server/core/common/images/column.gif");

$tpl->assign("default_graph_title", _("Default Graph Style"));
$tpl->assign("graph_colors_title", _("Graph colors"));
$tpl->assign("graph_misc", _("Other options"));
$tpl->assign("default_overview_title", _("Custom Overview"));

$helptext = "";
include_once("help.php");

foreach ($help as $key => $text) {
    $helptext .= '<span style="display:none" id="help:'.$key.'">'.$text.'</span>'."\n";
}
$tpl->assign("helptext", $helptext);

$valid = false;
if ($form->validate())	{
	$opt = new CentreonBAM_Options($pearDB, $oreon->user->user_id, $form);	
	if ($form->getSubmitValue("submitSaveChange")) {
		$opt->setUserOptions();
	}
	$o = NULL;
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
    $tpl->display("user_form.ihtml");
} else {
    $tpl->display("user_form-2.7.ihtml");   
}
?>
<script type='text/javascript' src='./modules/centreon-bam-server/core/common/javascript/initHelpTooltips.js'></script>
