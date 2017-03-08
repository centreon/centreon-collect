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

/* Pear library */
require_once "HTML/QuickForm.php";
require_once "HTML/QuickForm/advmultiselect.php";
require_once "HTML/QuickForm/Renderer/ArraySmarty.php";

$path = "./modules/centreon-bam-server/core/help/troubleshooter/";

$versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

if (!isset($centreon)) {
	exit();
}

$troubleshooter = new CentreonBAM_Troubleshooter($oreon, $pearDB);

$attrBtnInfo = array();
if (!$versionDesign) {
    $attrBtnInfo = array("class" => "btc bt_info");
}

$form = new HTML_QuickForm('Form', 'post', "?p=".$p);

$form->addElement("submit", "recheck", _("Recheck"), $attrBtnInfo);

$tpl = new Smarty();
$tpl =& initSmartyTpl($path . "template", $tpl); 

$tpl->assign("checkpoint", _("Checkpoint"));
$tpl->assign("status", _("Status"));
$tpl->assign("solution", _("Solution"));

$tpl->assign("plugin", _("Plugin"));
$tpl->assign("module_loaded", _("Module"));
$tpl->assign("xml_lib", _("XML Library"));
$tpl->assign("php_cli", _("PHP Configuration file (cli)"));

$module_loaded_status = $troubleshooter->checkModule();
$plugin_status = $troubleshooter->checkPlugin();
$xml_lib_status = $troubleshooter->checkXmlLib();
$cli_php_status = $troubleshooter->check_php_cli();

$plugin_solution = $troubleshooter->getPluginSolution($plugin_status);
$xml_lib_solution = $troubleshooter->getXmlLibSolution($xml_lib_status);
$module_loaded_solution = $troubleshooter->getModuleSolution($module_loaded_status);
$cli_php_solution = $troubleshooter->getPhpCliSolution($cli_php_status);

$color = array(_("OK") => "#55C53A", _("NOK") => "#F91E05");

$tpl->assign("plugin_solution", $plugin_solution);
$tpl->assign("module_loaded_solution", $module_loaded_solution);
$tpl->assign("xml_lib_solution", $xml_lib_solution);
$tpl->assign("cli_php_solution", $cli_php_solution);

$tpl->assign("plugin_status", $plugin_status);
$tpl->assign("module_loaded_status", $module_loaded_status);
$tpl->assign("xml_lib_status", $xml_lib_status);
$tpl->assign("cli_php_status", $cli_php_status);

$tpl->assign("title", _("Troubleshooter"));
$message = _("The troubleshooter is able to check if the Centreon BAM module is working properly. In case one of the checkpoints appear to be \"NOK\", please follow the solution and check again. If you encounter problems that are not listed below, please contact your administrator or your support.");
$tpl->assign("msg", $message);

$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
$form->accept($renderer);
$tpl->assign('form', $renderer->toArray());
$tpl->assign('color', $color);

// display
$tpl->display("troubleshooter.ihtml");
