<?php
/**
 * 
 */

// Defining path
$modulePath = realpath(dirname(__FILE__) . '/../');
$centreonPath = realpath($modulePath . '/../../');
$bamClassPath = realpath($modulePath . '/core/class/CentreonBam/');
$centreonClassPath = $centreonPath . '/class/';

// Require 
require_once $centreonClassPath . 'centreonDB.class.php';
require_once $bamClassPath . '/Ba.php';
require_once $bamClassPath . '/Kpi.php';
require_once $modulePath . '/core/common/functions.php';

// Init of needed Object
$centreonDB = new CentreonDB();
$centreonStorageDB = new CentreonDB('broker');
$businessActivityObj = new CentreonBam_Ba($centreonDB, null, $centreonStorageDB);
$kpiObj = new CentreonBam_Kpi($centreonDB);

// parameters
$numberOfBa = 2000;
$numberOfKpiPerBa = 5;
$baPrefix = 'test_ba_';
$baParams = array(
    'ba_warning' => '80',
    'ba_critical' => '70',
    'notif_interval' => '5',
    'additional_poller' => '',
    'id_reporting_period' => '1',
    'ba_group_list' => array('6'),
    'bam_contact' => array('3', '16'),
    'bam_activate' => array('bam_activate' => '1')
);
$kpiCommonParams = array(
    'config_mode' => '0',
    'kpi_type' => '0',
    'wImpact' => '4',
    'cImpact' => '5',
    'uImpact' => '6'
);

// Generate Ba
for ($i = 1; $i <= $numberOfBa; $i++) {
    $currentBaName = $baPrefix . $i;
    $businessActivityObj->setBaParams(array_merge(
        $baParams,
        array('ba_name' => $currentBaName, 'ba_desc' => $currentBaName)
    ));
    $businessActivityObj->insertBAInDB();
    $baId = $businessActivityObj->getBA_id($currentBaName);
    $kpiObj->insertKPIInDB(array_merge(
        $kpiCommonParams,
        array('host_id' => '198', 'svc_id' => '1465', 'ba_id' => $baId)
    ));
    $kpiObj->insertKPIInDB(array_merge(
        $kpiCommonParams,
        array('host_id' => '205', 'svc_id' => '1577', 'ba_id' => $baId)
    ));
    $kpiObj->insertKPIInDB(array_merge(
        $kpiCommonParams,
        array('host_id' => '208', 'svc_id' => '1625', 'ba_id' => $baId)
    ));
    $kpiObj->insertKPIInDB(array_merge(
        $kpiCommonParams,
        array('host_id' => '210', 'svc_id' => '1657', 'ba_id' => $baId)
    ));
    $kpiObj->insertKPIInDB(array_merge(
        $kpiCommonParams,
        array('host_id' => '214', 'svc_id' => '1721', 'ba_id' => $baId)
    ));
}

