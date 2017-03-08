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

function recursiveTree($pearDB,$pearDBndo,$baObj,$metaObj, $id_indicator_ba, $statusSelected = null) {
    $query = "select ba.*, ba.current_status as ba_current_status, kpi.*, s.service_description, h.host_name, ba2.name as indicator_ba_name, boo.name as boolean_name, m.meta_name from mod_bam ba "
        . "left join mod_bam_kpi kpi on ba.ba_id = kpi.id_ba "
        . "left join host h on (kpi.host_id = h.host_id AND h.host_activate = '1') "
        . "left join service s on (kpi.service_id = s.service_id AND s.service_activate = '1') "
        . "left join mod_bam ba2 on (kpi.id_indicator_ba = ba2.ba_id AND ba2.activate = '1') "
        . "left join mod_bam_boolean boo on (kpi.boolean_id = boo.boolean_id AND boo.activate = '1') "
        . "left join meta_service m on (kpi.meta_id = m.meta_id AND m.meta_activate = '1') "
        . "where ba.ba_id = ".$id_indicator_ba . " AND kpi.activate = '1' ORDER BY ba.name, boolean_name, host_name, service_description, indicator_ba_name, meta_name " ;
    $DBRES2 = $pearDB->query($query);
    $data = array();
    $flagStatusFound = false; 
    
    if (empty($statusSelected)){
       $flagStatusFound = true; 
    }
    while ($row = $DBRES2->fetchRow()) {
        $data['name'] = $row['name'];
        $data['status'] = $row['ba_current_status'];
        $data['url'] = './main.php?p=207&o=d&ba_id='.$row['ba_id'];
        $data['type'] = "ba";
        $data['icon'] = "./modules/centreon-bam-server/core/common/img/ba.png";
        unset($dataRow);
        
        $sDescription = trim($row['service_description']);
        if ($row['kpi_type'] == "0" && !empty($sDescription)){
            $svcId = $row['service_id'];
            if (!empty($statusSelected) && in_array($row['current_status'],$statusSelected)){
                $dataRow = array();
                $dataRow['name'] = $row['host_name'].' '.$row['service_description'];
                $dataRow['status'] = $row['current_status'];
                $dataRow['url'] = './main.php?p=20201&o=svcd&host_name='.$row['host_name'].'&service_description='.$row['service_description'];
                $dataRow['type'] = "kpi";
                $dataRow['icon'] = "./img/icons/service.png";
                $flagStatusFound = true;
            } else if (empty($statusSelected)) {
                $dataRow = array();
                $dataRow['name'] = $row['host_name'].' '.$row['service_description'];
                $dataRow['status'] = $row['current_status'];
                $dataRow['url'] = './main.php?p=20201&o=svcd&host_name='.$row['host_name'].'&service_description='.$row['service_description'];
                $dataRow['type'] = "kpi";
                $dataRow['icon'] = "./img/icons/service.png";
            }
        } else if ($row['kpi_type'] == "1") {
            $svcId = $metaObj->getCentreonServiceMetaId($row['meta_id'], $pearDBndo);
            $dataRow['name'] = $row['meta_name'];
            $dataRow['status'] = $row['current_status'];
            $dataRow['url'] = './main.php?p=20206&o=meta';
            $dataRow['type'] = "kpi";
            $dataRow['icon'] = "./img/icons/service.png";
        } else if ($row['kpi_type'] == "3") {
            if (!empty($statusSelected) && in_array($row['current_status'],$statusSelected)){
                $dataRow = array();
                $dataRow['name'] = $row['boolean_name'];
                $dataRow['status'] = $row['current_status'];
                $dataRow['type'] = "kpi";
                $dataRow['icon'] = "./img/icons/service.png";
                $flagStatusFound = true;
            } else if (empty($statusSelected)) {
                $dataRow = array();
                $dataRow['name'] = $row['boolean_name'];
                $dataRow['status'] = $row['current_status'];
                $dataRow['type'] = "kpi";
                $dataRow['icon'] = "./img/icons/service.png";
            }
        } else if($row['kpi_type'] == "2") {
            $childrens = recursiveTree($pearDB,$pearDBndo,$baObj,$metaObj, $row['id_indicator_ba'], $statusSelected);
            if ($childrens['statusFound']) {
                $dataRow = array();
                $dataRow = $childrens['data'];
                $flagStatusFound = true;
            }
        }
        if ($flagStatusFound && isset($dataRow)) {
            $data['children'][] = $dataRow;
        }
    }
    return array('data' => $data, 'statusFound' => $flagStatusFound);
}

require_once("../../../centreon-bam-server.conf.php");
require_once("../..//common/functions.php");

if (file_exists($centreon_path."www/class/centreonDuration.class.php")) {
    require_once $centreon_path."www/class/centreonDuration.class.php";
    $durationObj = new CentreonDuration();
}

require_once("../../common/header.php");

require_once $centreon_path . 'www/class/centreon.class.php';
require_once $centreon_path . 'www/class/centreonWidget.class.php';
require_once $centreon_path . 'www/class/centreonUtils.class.php';

require_once $centreon_path ."GPL_LIB/Smarty/libs/Smarty.class.php";

$centreon = $_SESSION['centreon'];
$page = 0;

if (isset($_REQUEST['page'])){
    $page = $_REQUEST['page'];
}

$db = new CentreonDB();
$dbb = new CentreonDB("centstorage");
$path = $centreon_path . "www/modules/centreon-bam-server/core/dashboard/treeview/";

$template = new Smarty();
$template = initSmartyTplForPopup($path, $template, "./", $centreon_path);

$pearDB = new CentreonBam_Db();
$pearDBO = new CentreonBam_Db("centstorage");

$ba_acl = new CentreonBam_Acl($pearDB, $oreon->user->user_id);
$tpObj = new CentreonBam_TimePeriod($pearDB);
$buffer = new CentreonBam_Xml();
$bvObj = new CentreonBam_BaGroup($pearDB);
$optionsObj = new CentreonBam_Options($pearDB, $oreon->user->user_id);
$baObj = new CentreonBam_Ba($pearDB, null, $pearDBO);
$metaObj = new CentreonBam_Meta($pearDB);
$statusSelected = array();

$flagStatusFound = false; 
if (empty($statusSelected)){
   $flagStatusFound = true; 
}

$data = recursiveTree($pearDB,$pearDBO,$baObj,$metaObj,intval(htmlentities($_GET["ba_id"])));
$template->assign('data', json_encode($data['data']));

$template->assign('centreon_web_path', trim($centreon->optGen['oreon_web_path'], "/"));
$template->display('ba_tree.html');
