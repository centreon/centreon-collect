<?php

// Set include path
$module_path = $centreon_path . "www/modules/centreon-pp-manager/";
$templatePath = "./modules/centreon-pp-manager/core/templates/";

// include pp manager class
include_once $module_path.'/core/class/centreonDBManager.class.php';
include_once $module_path.'/core/class/centreonPluginPack/PPLocal.class.php';

// Initialize Smarty
$tpl = new Smarty();
$tpl = initSmartyTpl($templatePath, $tpl);

try {
    // Get Plugin pack slug
    $pluginPackSlug = filter_input(INPUT_GET, 'slug');

    // Retrieve monitoring procedure
    $pluginPackObj = new \CentreonPluginPackManager\PPLocal($pluginPackSlug, new CentreonDB());
    $pluginPackFromDb = $pluginPackObj->getProperties();
    $tpl->assign('monitoringProcedure', $pluginPackFromDb['monitoring_procedure']);
    $tpl->assign('name', $pluginPackFromDb['name']);

    // Display documentation template
    $tpl->display("displayDocumentation.tpl");
} catch (Exception $ex) {

}
