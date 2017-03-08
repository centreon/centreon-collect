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

// Get Business Activty list
$businessActivityObj = new CentreonBam_Ba($centreonDb, null, $centreonStorageDB);
$baList = $businessActivityObj->getBA_list($bamAclObj->queryBuilder('AND', 'ba_id', $bamAclObj->getBaStr()));
$baId = filter_input(INPUT_GET, 'ba_id');
if (!is_numeric($baId)) {
    $baId = $businessActivityObj->getBA_id($baId);
}
$hostId = $businessActivityObj->getCentreonHostBaId($baId);
$serviceId = $businessActivityObj->getCentreonServiceBaId($baId, $hostId);

$smartyTemplateObj->assign('ba_id', $baId);
$smartyTemplateObj->assign('host_id', $hostId);
$smartyTemplateObj->assign('service_id', $serviceId);
$smartyTemplateObj->assign('ba_name', addslashes($businessActivityObj->getBA_Name($baId)));
$smartyTemplateObj->assign('ba_list', $baList);

$urlKpiStatusListService = './include/common/webServices/rest/internal.php?object=centreon_bam_kpi&action=statusList&bv_filter=0';
$urlKpiStatusListService .= '&sid=' . session_id();
$urlKpiStatusListService .= '&p=' . $p;
$urlKpiStatusListService .= '&num=' . $num;
$urlKpiStatusListService .= '&limit=' . $limit;
$smartyTemplateObj->assign('kpiStatusListService', $urlKpiStatusListService);
$smartyTemplateObj->assign('sessionId', session_id());

$currentTime = time();
$smartyTemplateObj->assign('currentTime', $currentTime);
$smartyTemplateObj->assign('dailyTime', ($currentTime - ((60*60*24))));
$smartyTemplateObj->assign('weeklyTime', ($currentTime - ((60*60*24*7))));
$smartyTemplateObj->assign('monthlyTime', ($currentTime - ((60*60*24*31))));
$smartyTemplateObj->assign('yearlyTime', ($currentTime - ((60*60*24*365))));

$smartyTemplateObj->assign("reporting_link", "./main.php?p=20702&period=yesterday&item=" . $_GET['ba_id']);
$smartyTemplateObj->assign("reporting_link_allowed", isset($centreon->user->access->topology["20702"]));
$smartyTemplateObj->assign("graph_link_allowed", isset($centreon->user->access->topology["20401"]));

$smartyTemplateObj->assign(
    'svcIndex',
    getMyIndexGraph4Service($baHostNameInCentreon, "ba_" . $baId, $centreonStorageDB)
);

// Display business activity
$smartyTemplateObj->display('businessActivity.html');
