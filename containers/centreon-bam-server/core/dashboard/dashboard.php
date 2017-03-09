<?php
/**
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

// Load common files
$modulePath = realpath(dirname(__FILE__) . '/../../') . '/';
require_once $modulePath . "core/common/header.php";
require_once $modulePath . "core/common/functions.php";

// Load QuickForm
require_once "HTML/QuickForm.php";
require_once "HTML/QuickForm/select2.php";
require_once 'HTML/QuickForm/Renderer/ArraySmarty.php';

// Init objects
$centreonDb = new CentreonBam_Db();
$centreonStorageDB = new CentreonBam_Db("centstorage");
$baHostNameInCentreon = getCentreonBaHostName($centreonDb);
$bamUtilsObj = new CentreonBam_Utils($pearDB);

// Init user resources
$userId = $oreon->user->user_id;
$bamAclObj = new CentreonBam_Acl($centreonDb, $userId);
$bamUserOptionsObj = new CentreonBam_Options($centreonDb, $userId);
$userOptions = $bamUserOptionsObj->getUserOptions();

// Get current action
$o = filter_input(INPUT_POST, 'o');
if (($o === false) || is_null($o)) {
    $o = filter_input(INPUT_GET, 'o');
}


$sectionFilename = '';
$templatePath = '';
switch ($o) {
    case 'd':
        $sectionFilename .= $modulePath . "core/dashboard/businessActivity/businessActivity.php";
        $templatePath .= $modulePath . "core/dashboard/businessActivity/template/";
        break;
    
    default:
        $sectionFilename .= $modulePath . "core/dashboard/javascript/dashboard_js.php";
        $templatePath .= $modulePath . "core/dashboard/template/";
        break;
}

// Init Smarty
$smartyObj = new Smarty();
$smartyTemplateObj = initSmartyTpl($templatePath, $smartyObj);

$smartyTemplateObj->assign("monitoring_img", "./modules/centreon-bam-server/core/common/img/ba.png");
$smartyTemplateObj->assign("previous_img", "./modules/centreon-bam-server/core/common/images/find_previous.png");
$smartyTemplateObj->assign("actions_img", "./modules/centreon-bam-server/core/common/images/star_yellow.gif");
$smartyTemplateObj->assign("loading_img", "./modules/centreon-bam-server/core/common/images/ajax-loader.gif");

$smartyTemplateObj->assign("monitoring_label", _("Business Activity Monitoring"));
$smartyTemplateObj->assign("actions", _("Actions"));
$smartyTemplateObj->assign("go_back", _("Back"));
$smartyTemplateObj->assign("go_reporting", _("Reporting view"));
$smartyTemplateObj->assign("views", _("Views"));
$smartyTemplateObj->assign("go_to_overview", _("Overview"));
$smartyTemplateObj->assign("go_to_previous", _("Previous View"));
$smartyTemplateObj->assign("misc", _("Misc"));
$smartyTemplateObj->assign("business_view", _("Business Views : "));
$smartyTemplateObj->assign("performance_label", _("Performances"));
$smartyTemplateObj->assign("loading_label", _("Loading..."));

/* Downtime */
$smartyTemplateObj->assign('startLabel', _('Start'));
$smartyTemplateObj->assign('endLabel', _('End'));
$smartyTemplateObj->assign('defaultStart', date('Y/m/d H:i', time()));
$smartyTemplateObj->assign('defaultEnd', date('Y/m/d H:i', time() + $oreon->optGen["monitoring_dwt_duration"]));
$smartyTemplateObj->assign('durationLabel', _("Duration"));
$smartyTemplateObj->assign('fixedLabel', _("Fixed ?"));
$smartyTemplateObj->assign('fixedDefault', ($oreon->optGen["monitoring_dwt_fixed"] == 1 ? 'checked' : ''));
$smartyTemplateObj->assign('authorLabel', _("Author"));
$smartyTemplateObj->assign('authoralias', $oreon->user->get_alias());
$smartyTemplateObj->assign('defaultDuration', $oreon->optGen["monitoring_dwt_duration"]);
$smartyTemplateObj->assign('secondsLabel', _('seconds'));
$smartyTemplateObj->assign('scheduleLabel', _('Schedule'));
$smartyTemplateObj->assign('commentLabel', _('Comment'));

$smartyTemplateObj->assign('defaultComment', sprintf('Downtime scheduled by %s', $oreon->user->get_alias()));

require_once $sectionFilename;
