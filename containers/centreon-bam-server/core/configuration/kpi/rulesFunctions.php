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

function testKPIConfigurationStatus() {
    global $pearDB;
    global $form;

    if (!isset($form)) {
        return false;
    }
    
    $kpi_type = $form->getSubmitValue('kpi_type');

    if ($kpi_type == 0) {
        if (!isset($_POST['svc_select'])) {
            return false;
        }
        $sql = "SELECT service_activate 
            FROM service 
            WHERE service_id = " . $pearDB->escape($_POST['svc_select']);
        $res = $pearDB->query($sql);
        $row = $res->fetchRow();
        if (!$row['service_activate']) {
            return false;
        }
    }
    return true;
}

function testKPIExistence() {
    global $pearDB;
    global $form;

    if (!isset($form)) {
        return false;
    }

    $ba_list = array();
    $id = $form->getSubmitValue('kpi_id');
    $ba_list = $form->getSubmitValue('ba_list');
    $kpi_type = $form->getSubmitValue('kpi_type');

    $ba_str = "";
    if (count($ba_list)) {
        foreach ($ba_list as $key => $value) {
            if ($ba_str) {
                $ba_str .= ", ";
            }
            $ba_str .= "'" . $value . "'";
        }
    }
    if ($ba_str == "") {
        $ba_str = "''";
    }

    $query = "SELECT * FROM `mod_bam_kpi` " .
            "WHERE kpi_type = '" . $kpi_type . "' " .
            "AND kpi_id != '" . $id . "' " .
            "AND id_ba IN (" . $ba_str . ") ";
    if ($kpi_type == 0) {
        $query .= "AND host_id = '" . $_POST['kpi_select'] . "' " .
                "AND service_id = '" . $_POST['svc_select'] . "'";
    } elseif ($kpi_type == 1) {
        $query .= "AND meta_id = '" . $_POST['kpi_select'] . "'";
    } elseif ($kpi_type == 2) {
        $query .= "AND id_indicator_ba = '" . $_POST['kpi_select'] . "'";
    } elseif ($kpi_type == 3) {
        $query .= "AND boolean_id = " . $pearDB->escape($_POST['kpi_select']);
    }
    $DBRESULT = $pearDB->query($query);
    if ($DBRESULT->numRows()) {
        return false;
    }
    return true;
}

function checkSelfBaAsKpi() {
    global $form;

    $ba_list = array();
    $ba_list = $form->getSubmitValue('ba_list');
    $kpi_type = $form->getSubmitValue('kpi_type');
    $kpi_id = $_POST['kpi_select'];
    if ($kpi_type == "2") {
        if (count($ba_list)) {
            foreach ($ba_list as $key => $value) {
                if ($value == $kpi_id) {
                    return false;
                }
            }
        }
    }
    return true;
}

function checkSelectedValues() {
    if (isset($_POST['kpi_select']) && !$_POST['kpi_select']) {
        return false;
    }
    if (isset($_POST['svc_select']) && !$_POST['svc_select']) {
        return false;
    }
    return true;
}

function checkInfiniteLoop($ba_id = null, $impacted_ba_id = null) {
    global $pearDB;
    global $form;

    $kpi_type = $form->getSubmitValue('kpi_type');
    if ($kpi_type != "2") {
        return true;
    }

    $ba_list = array();
    if (is_null($impacted_ba_id)) {
        if (!isset($form)) {
            return false;
        }
        $ba_list = $form->getSubmitValue('ba_list');
        $ba_id = $form->getSubmitValue('kpi_select');
    } else {
        $ba_list[] = $impacted_ba_id;
    }

    foreach ($ba_list as $impacted_ba_id) {
        $query = "SELECT id_ba FROM `mod_bam_kpi` WHERE id_indicator_ba = '" . $impacted_ba_id . "' AND kpi_type = '2' ";
        $DBRES = $pearDB->query($query);
        while ($row = $DBRES->fetchRow()) {
            if ($row['id_ba'] == $ba_id || !checkInfiniteLoop($ba_id, $row['id_ba'])) {
                return false;
            }
        }
    }

    return true;
}

function checkInstanceBa()
{
    global $form;
    global $pearDB;
    $aBaSelected = array();
    $bCheck = true;
   
    if (!isset($form)) {
        return false;
    }
    if (!in_array($form->getSubmitValue('kpi_type'), array('0', '2', '3'))  ) {
       return $bCheck; 
    }
    
    if ($form->getSubmitValue('ba_list')) {
        $aBaSelected = $form->getSubmitValue('ba_list');
    }
    $oKpi = new CentreonBam_Kpi($pearDB);
    
    if ($form->getSubmitValue('kpi_type') == '0') {
        $host = $form->getSubmitValue('kpi_select');
        $bCheck = $oKpi->checkKpiTypeService($aBaSelected, $host);
    } else if ($form->getSubmitValue('kpi_type') == '2') {
        $baIndicator = $form->getSubmitValue('kpi_select');      
        $bCheck = $oKpi->checkKpiTypeBa($aBaSelected, $baIndicator);
    } else if ($form->getSubmitValue('kpi_type') == '3') {
        $rules = $form->getSubmitValue('kpi_select');
        $bCheck = $oKpi->checkKpiTypeBoolean($aBaSelected, $rules);
    }

    return $bCheck;
}

function checkObjectInstance()
{
    global $form;
    global $pearDB;
    $aBaSelected = array();
    $bCheck = true;
    $iHostId = 0;
    
    if (!isset($form)) {
        return false;
    }
 
    $select = $form->getSubmitValue('select');
     
    foreach ($select as $key => $value) {
        list($iHostId, $servie) = explode(";", $key);
    }
    
    $oKpi = new CentreonBam_Kpi($pearDB);
    
    if ($form->getSubmitValue('ba_list')) {
        $aBaSelected = $form->getSubmitValue('ba_list');
    }
    
    if ($form->getSubmitValue('obj_type') == '0') {//Host
        $host = $form->getSubmitValue('host_list');
        $bCheck = $oKpi->checkKpiTypeService($aBaSelected, $host);
    } else if ($form->getSubmitValue('obj_type') == '1') {//Host group
        $hostGroup = $form->getSubmitValue('hg_list');
        $bCheck = $oKpi->checkKpiHostGroup($aBaSelected, $hostGroup, $iHostId);
    } else if ($form->getSubmitValue('obj_type') == '2') {//Service group
        $serviceGroup = $form->getSubmitValue('sg_list');
        $bCheck = $oKpi->checkKpiServiceGroup($aBaSelected, $serviceGroup, $iHostId);
    }
    return $bCheck;
 }

?>
