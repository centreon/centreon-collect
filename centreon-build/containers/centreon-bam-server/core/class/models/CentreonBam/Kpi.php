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

define('CRITICITY_NULL', 0);
define('CRITICITY_WEAK', 1);
define('CRITICITY_MINOR', 2);
define('CRITICITY_MAJOR', 3);
define('CRITICITY_CRITICAL', 4);
define('CRITICITY_BLOCKING', 5);

/*
 *  Checks if Group already exists
 *  if it exists, function returns false
 *  otherwise, it returns true
 */

/*
 * Class that contains all methods for managing BA
 */

class CentreonBam_Kpi {

    protected $_form;
    protected $_host;
    protected $_service;
    protected $_meta;
    protected $_ba;
    protected $_db;
    protected $_dbNdo;
    protected $_criticity;
    protected $_criticityCode;
    protected $_criticityImpact;
    protected $_criticityColor;
    protected $_defaultImpacts;
    protected $_expr;

    /*
     *  Constructor
     */

    function __construct($db, $form = null, $hObj = null, $sObj = null, $metaObj = null, $baObj = null)
    {
        global $centreon_path;

        require_once ($centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/Loader.php");

        $cBamLoader = new CentreonBam_Loader($centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/");
        $cLoader = new CentreonBam_Loader($centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/");
        
        $this->_dbNdo = new CentreonBam_Db("centstorage");
        

        $this->_expr = "a-zA-Z0-9àâçéèêëîïôûùüÿñæœ\:\#\/\_\-\.\ ";
        $this->_db = $db;
        $this->_form = $form;
        if (!is_null($hObj)) {
            $this->_host = $hObj;
        } else {
            $this->_host = new CentreonBam_Host($db);
        }
        if (!is_null($sObj)) {
            $this->_service = $sObj;
        } else {
            $this->_service = new CentreonBam_Service($db);
        }
        if (!is_null($metaObj)) {
            $this->_meta = $metaObj;
        } else {
            $this->_meta = new CentreonBam_Meta($db);
        }
        if (!is_null($baObj)) {
            $this->_ba = $baObj;
        } else {
            $this->_ba = new CentreonBam_Ba($db, null, $this->_dbNdo);
        }
        $this->_setCriticityLabel();
        $this->_setCriticityValues();
        $this->_setDefaultImpacts();
    }

    /**
     *
     *
     */
    protected function _setDefaultImpacts()
    {
        $query = "SELECT * FROM mod_bam_impacts";
        $res = $this->_db->query($query);
        while ($row = $res->fetchRow()) {
            $this->_defaultImpacts[$row['code']] = $row['impact'];
        }
    }

    /**
     *
     *
     */
    protected function _setCriticityLabel()
    {
        $this->_criticity = array();
        $this->_criticity[CRITICITY_NULL] = _('Null');
        $this->_criticity[CRITICITY_WEAK] = _('Weak');
        $this->_criticity[CRITICITY_MINOR] = _('Minor');
        $this->_criticity[CRITICITY_MAJOR] = _('Major');
        $this->_criticity[CRITICITY_CRITICAL] = _('Critical');
        $this->_criticity[CRITICITY_BLOCKING] = _('Blocking');
    }

    /**
     *
     *
     */
    protected function _setCriticityValues()
    {
        $res = $this->_db->query("SELECT * FROM mod_bam_impacts");
        $this->_criticityColor = array();
        $this->_criticityImpact = array();
        $this->_criticityCode = array();
        while ($row = $res->fetchRow()) {
            $this->_criticityColor[$row['id_impact']] = $row['color'];
            $this->_criticityImpact[$row['id_impact']] = $row['impact'];
            $this->_criticityCode[$row['id_impact']] = $row['code'];
        }
    }

    /*
     *  Checks if there is an infinite loop
     */

    private function checkInfiniteLoop($kpi_ba_id, $ba_id)
    {
        $query = "SELECT id_indicator_ba FROM `mod_bam_kpi` WHERE id_ba = '" . $kpi_ba_id . "' AND kpi_type = '2' ";
        $res = $this->_db->query($query);
        while ($row = $res->fetchRow()) {
            if (isset($row['id_indicator_ba']) && $row['id_indicator_ba'] != "") {
                if ($row['id_indicator_ba'] == $ba_id || !$this->checkInfiniteLoop($row['id_indicator_ba'], $ba_id)) {
                    return false;
                }
                //return ($this->checkInfiniteLoop($row['id_indicator_ba'], $ba_id));
            }
        }
        return true;
    }

    /*
     *  Tests if KPI already exists
     */

    public function testKPIExistence($ba_id, $kpi_type, $elem = NULL)
    {
        $query = "SELECT * FROM `mod_bam_kpi` " .
                "WHERE kpi_type = '" . $kpi_type . "' " .
                "AND id_ba = '" . $ba_id . "' ";
        if ($kpi_type == 0) {
            if (!isset($elem)) {
                $query .= "AND host_id = '" . $_POST['kpi_select'] . "' " .
                        "AND service_id = '" . $_POST['svc_select'] . "'";
            } else {
                $query .= "AND host_id = '" . $elem['host_id'] . "' " .
                        "AND service_id = '" . $elem['svc_id'] . "'";
            }
        } elseif ($kpi_type == 1) {
            $query .= "AND meta_id = '" . $_POST['kpi_select'] . "'";
        } elseif ($kpi_type == 2) {
            if (!$this->checkInfiniteLoop($_POST['kpi_select'], $ba_id) || ($ba_id == $_POST['kpi_select'])) {
                return false;
            }
            $query .= "AND id_indicator_ba = '" . $_POST['kpi_select'] . "'";
        } elseif ($kpi_type == 3) {
            $query .= " AND boolean_id = " . $this->_db->escape($_POST['kpi_select']);
        }
        $result = $this->_db->query($query);
        if ($result->numRows()) {
            return false;
        }
        return true;
    }

    /**
     *   Inserts KPI
     */
    public function insertKPIInDB($elem = null, $kpiId = null, $extraParams = null)
    {
        $impact = array();
        $report = "";

        if (!isset($elem)) {
            $configType = $_POST['config_mode']['config_mode'];
            $kpi_type = $_POST['kpi_type'];
            if ($configType) {
                $impact["warning"] = $_POST['warning_impact'];
                $impact["unknown"] = $_POST['unknown_impact'];
                if ($kpi_type == 3) {
                    $impact["critical"] = $_POST['boolean_impact'];
                } else {
                    $impact["critical"] = $_POST['critical_impact'];
                }
            } else {
                $impact["warning"] = $_POST['warning_impact_regular'];
                $impact["unknown"] = $_POST['unknown_impact_regular'];
                if ($kpi_type == 3) {
                    $impact["critical"] = $_POST['boolean_impact_regular'];
                } else {
                    $impact["critical"] = $_POST['critical_impact_regular'];
                }
            }
            $query2 = "";
            $query2 .= ($kpi_type == 0) ? "'" . $_POST['kpi_select'] . "', " : "NULL, ";
            $query2 .= ($kpi_type == 0) ? "'" . $_POST['svc_select'] . "', " : "NULL, ";
            $query2 .= ($kpi_type == 1) ? "'" . $_POST['kpi_select'] . "', " : "NULL, ";
            $query2 .= ($kpi_type == 2) ? "'" . $_POST['kpi_select'] . "', " : "NULL, ";
            $query2 .= ($kpi_type == 3) ? "'" . $_POST['kpi_select'] . "'" : "NULL";
            $tab = $this->_form->getSubmitValue("ba_list");
        } else {
            $configType = $elem['config_mode'];
            $kpi_type = $elem['kpi_type'];
            $impact["warning"] = $elem['wImpact'];
            $impact["critical"] = $elem['cImpact'];
            $impact["unknown"] = $elem['uImpact'];

            $query2 = "";
            $query2 .= ($kpi_type == 0) ? "'" . $elem['host_id'] . "', " : "NULL, ";
            $query2 .= ($kpi_type == 0) ? "'" . $elem['svc_id'] . "', " : "NULL, ";
            $query2 .= ($kpi_type == 1) ? "'" . $elem['meta_id'] . "', " : "NULL, ";
            $query2 .= ($kpi_type == 2) ? "'" . $elem['ba_id_kpi'] . "', " : "NULL, ";
            $query2 .= ($kpi_type == 3) ? "'" . $elem['boolean_id'] . "'" : "NULL";
            $tab = array(0 => $elem['ba_id']);
        }

        $kpiKey = "";
        $extraKeys = "";
        $extraValues = "";
        if (!is_null($extraParams)) {
            $extraKeys = ", current_status, last_state_change, in_downtime, last_impact";
            $extraValues = ", {$extraParams['current_status']}, {$extraParams['last_state_change']}, {$extraParams['in_downtime']}, {$extraParams['last_impact']}";
        }

        foreach ($tab as $key => $ba_id) {
            if ($this->testKPIExistence($ba_id, $kpi_type, $elem)) {
                $query = "INSERT INTO `mod_bam_kpi` ";
                if (!is_null($kpiId)) {
                    $kpiKey = ", kpi_id";
                }
                if ($configType) {
                    $query .= "(id_ba, config_type, drop_warning, drop_critical, drop_unknown, 
                        kpi_type, host_id, service_id, meta_id, id_indicator_ba, boolean_id $kpiKey $extraKeys) VALUES (";
                } else {
                    $query .= "(id_ba, config_type, drop_warning_impact_id, drop_critical_impact_id, drop_unknown_impact_id, 
                        kpi_type, host_id, service_id, meta_id, id_indicator_ba, boolean_id $kpiKey $extraKeys) VALUES (";
                }
                $query .= "'" . $ba_id . "', ";
                $query .= "'" . $configType . "', ";

                $query .= "'" . $impact['warning'] . "', ";
                $query .= "'" . $impact['critical'] . "', ";
                $query .= "'" . $impact['unknown'] . "', ";
                $query .= "'" . $kpi_type . "', ";
                $query .= $query2;

                if (!is_null($kpiId)) {
                    $query .= ", $kpiId";
                    $kpiId = null;
                    $kpiKey = "";
                }

                $query .= $extraValues;

                $query .= ")";
                $this->_db->query($query);
                if (isset($elem['line'])) {
                    $report .= sprintf(_("OK - Line %s successfully imported") . "<br/>", $elem['line']);
                }
            } elseif (isset($elem['line'])) {
                $report .= sprintf(_("Error - KPI already attached to Business Activity on line %s"), $elem['line']) . "<br/>";
            }
        }
        return $report;
    }

    /*
     *  Updates KPI
     */

    public function updateKPI($kpi_id)
    {
        /* extra params that are not present in kpi form */
        $sql = "SELECT current_status, last_state_change, in_downtime, last_impact 
            FROM mod_bam_kpi WHERE kpi_id = " . $this->_db->escape($kpi_id);
        $stmt = $this->_db->query($sql);
        $row = $stmt->fetchRow();
        $extraParams = array();
        foreach ($row as $k => $v) {
            $extraParams[$k] = ($v != "") ? $v : "NULL";
        }

        /* delete kpi */
        $this->deleteKPI($kpi_id);

        $this->insertKPIInDB(null, $kpi_id, $extraParams);
    }

    /*
     *  Sets status (enable/disable) of a KPI
     */

    public function setActivate($kpi_id, $status, $multiSelect = null)
    {
        $list = "";
        if (isset($multiSelect)) {
            foreach ($multiSelect as $key => $value) {
                if ($list != "") {
                    $list .= ", ";
                }
                $list .= "'" . $key . "'";
            }
            if ($list == "") {
                $list = "''";
            }
        } else {
            $list = "'" . $kpi_id . "'";
        }
        $query = "UPDATE `mod_bam_kpi` " .
                "SET activate = '" . $status . "' " .
                "WHERE kpi_id IN ($list)";
        $this->_db->query($query);
        
        //Desactive/Active boolean_rule
        $this->setStatusBolleanByKpiId($list, $status);
    }

    /*
     *  Deletes a kpi from database
     */

    public function deleteKPI($id_kpi, $multiSelect = null)
    {
        $list = "";
        if (isset($multiSelect)) {
            foreach ($multiSelect as $key => $value) {
                if ($list != "") {
                    $list .= ", ";
                }
                $list .= "'" . $key . "'";
            }
            if ($list == "") {
                $list = "''";
            }
        } else {
            $list = "'" . $id_kpi . "'";
        }
        $query = "DELETE FROM `mod_bam_kpi` WHERE kpi_id IN ($list)";
        $this->_db->query($query);
    }

    /*
     *  Test if KPI exists for the BA
     */

    private function testMultipleKPIExistence($host_id, $svc_id, $ba_id) {
        $query = "SELECT * FROM `mod_bam_kpi` WHERE host_id = '" . $host_id . "' AND service_id = '" . $svc_id . "' AND id_ba = '" . $ba_id . "'";
        $res = $this->_db->query($query);
        if ($res->numRows()) {
            return false;
        }
        return true;
    }

    /*
     *  Inserts multiple KPI
     */

    public function insertMultipleKPI($hostSvc, $form)
    {
        $tab = $form->getSubmitValue("ba_list");
        $tmp = explode(";", $hostSvc);
        $host_id = $tmp[0];
        $svc_id = $tmp[1];
        $query = "";
        $configType = null;
        if (isset($_POST['w_' . $host_id . '_' . $svc_id]) && isset($_POST['c_' . $host_id . '_' . $svc_id]) && isset($_POST['u_' . $host_id . '_' . $svc_id])) {
            $configType = 1;
            $wImpact = $_POST['w_' . $host_id . '_' . $svc_id];
            $cImpact = $_POST['c_' . $host_id . '_' . $svc_id];
            $uImpact = $_POST['u_' . $host_id . '_' . $svc_id];
        } elseif (isset($_POST['bw_' . $host_id . '_' . $svc_id]) && isset($_POST['bc_' . $host_id . '_' . $svc_id]) && isset($_POST['bu_' . $host_id . '_' . $svc_id])) {
            $configType = 0;
            $wImpact = $_POST['bw_' . $host_id . '_' . $svc_id];
            $cImpact = $_POST['bc_' . $host_id . '_' . $svc_id];
            $uImpact = $_POST['bu_' . $host_id . '_' . $svc_id];
        }
        foreach ($tab as $key => $ba_id) {
            if ($this->testMultipleKPIExistence($host_id, $svc_id, $ba_id)) {
                if ($query != "") {
                    $query .= ",";
                } else {
                    if ($configType) {
                        $query = "INSERT INTO `mod_bam_kpi` (kpi_type, host_id, service_id, id_ba, drop_warning, drop_critical, drop_unknown, config_type) VALUES ";
                    } else {
                        $query = "INSERT INTO `mod_bam_kpi` (kpi_type, host_id, service_id, id_ba, drop_warning_impact_id, drop_critical_impact_id, drop_unknown_impact_id, config_type) VALUES ";
                    }
                }
                $query .= "('0', '" . $host_id . "', '" . $svc_id . "', '" . $ba_id . "', '" . $wImpact . "', '" . $cImpact . "', '" . $uImpact . "', '" . $configType . "')";
            }
        }
        if ($query != "") {
            $this->_db->query($query);
        }
    }

    /*
     *
     */

    private function csvLoad_ba($lines)
    {
        $report = "";
        $i = 0;
        foreach ($lines as $line) {
            $i++;
            $line = str_replace("\"", "", $line);
            $line = str_replace("\'", "", $line);
            $line = trim($line);

            $matches = explode(';', $line);

            if (count($matches) == 5) {
                for ($z = 0; $z < 5; $z++) {
                    $matches[$z] = trim($matches[$z]);
                }
                $elem = array();
                $elem['kpi_type'] = "2";
                $elem['ba_id'] = $this->_ba->getBA_id($matches[0]);
                $elem['ba_id_kpi'] = $this->_ba->getBA_id($matches[1]);
                $elem['wImpact'] = $matches[2];
                $elem['cImpact'] = $matches[3];
                $elem['uImpact'] = $matches[4];
                $elem['line'] = $i;
                $elem['config_mode'] = 1;
                if ($elem['ba_id'] == null) {
                    $report .= sprintf(_("Error - Business Activity %s not found on line %s") . "<br/>", $matches[0], $i);
                } elseif ($elem['ba_id_kpi'] == null) {
                    $report .= sprintf(_("Error - Business Activity KPI %s not found on line %s") . "<br/>", $matches[1], $i);
                } else {
                    $report .= $this->insertKPIInDB($elem);
                }
            } else {
                $report .= sprintf(_("Error - Could not read line %s") . "<br/>", $i);
            }
        }
        return $report;
    }

    /*
     *
     */

    private function csvLoad_meta($lines)
    {
        $report = "";
        $i = 0;
        foreach ($lines as $line) {
            $i++;
            $line = str_replace("\"", "", $line);
            $line = str_replace("\'", "", $line);
            $line = trim($line);
            $matches = explode(';', $line);

            if (count($matches) == 5) {
                for ($z = 0; $z < 5; $z++) {
                    $matches[$z] = trim($matches[$z]);
                }
                $elem = array();
                $elem['kpi_type'] = "1";
                $elem['ba_id'] = $this->_ba->getBA_id($matches[0]);
                $elem['meta_id'] = $this->_meta->getMetaId($matches[1]);
                $elem['wImpact'] = $matches[2];
                $elem['cImpact'] = $matches[3];
                $elem['uImpact'] = $matches[4];
                $elem['line'] = $i;
                $elem['config_mode'] = 1;
                if ($elem['ba_id'] == null) {
                    $report .= sprintf(_("Error - Business Activity %s not found on line %s") . "<br/>", $matches[0], $i);
                } elseif ($elem['meta_id'] == null) {
                    $report .= sprintf(_("Error - Meta %s not found on line %s") . "<br/>", $matches[1], $i);
                } else {
                    $report .= $this->insertKPIInDB($elem);
                }
            } else {
                $report .= sprintf(_("Error - Could not read line %s") . "<br/>", $i);
            }
        }
        return $report;
    }

    /*
     *
     */

    private function csvLoad_regular($lines)
    {
        $report = "";
        $i = 0;
        foreach ($lines as $line) {
            $i++;
            $line = str_replace("\"", "", $line);
            $line = str_replace("\'", "", $line);
            $line = trim($line);
            $matches = explode(';', $line);

            if (count($matches) == 6) {
                for ($z = 0; $z < 6; $z++) {
                    $matches[$z] = trim($matches[$z]);
                }
                $elem = array();
                $elem['kpi_type'] = "0";
                $elem['ba_id'] = $this->_ba->getBA_id($matches[0]);
                $elem['host_id'] = $this->_host->getHostId($matches[1]);
                $elem['svc_id'] = $this->_service->getServiceId($matches[2], $matches[1]);
                $elem['wImpact'] = $matches[3];
                $elem['cImpact'] = $matches[4];
                $elem['uImpact'] = $matches[5];
                $elem['line'] = $i;
                $elem['config_mode'] = 1;
                if ($elem['ba_id'] == null) {
                    $report .= sprintf(_("Error - Business Activity %s not found on line %s") . "<br/>", $matches[0], $i);
                } elseif ($elem['host_id'] == null) {
                    $report .= sprintf(_("Error - Host %s not found on line %s") . "<br/>", $matches[1], $i);
                } elseif ($elem['svc_id'] == null) {
                    $report .= sprintf(_("Error - Service %s not found on line %s") . "<br/>", $matches[2], $i);
                } else {
                    $report .= $this->insertKPIInDB($elem);
                }
            } else {
                $report .= sprintf(_("Error - Could not read line %s") . "<br/>", $i);
            }
        }
        return $report;
    }

    /*
     *  CSV Loader
     */

    public function csvLoad($buffer, $kpiType)
    {
        if ((!is_array($buffer) && strlen($buffer) == 0) || (is_array($buffer) && !count($buffer))) {
            return null;
        }
        if (!is_array($buffer)) {
            $lines = explode("\n", $buffer);
        } else {
            $lines = $buffer;
        }

        switch ($kpiType) {
            case "0" : $report = $this->csvLoad_regular($lines);
                break;
            case "1" : $report = $this->csvLoad_meta($lines);
                break;
            case "2" : $report = $this->csvLoad_ba($lines);
                break;
        }
        return $report;
    }

    /**
     *  Gets output from centreon broker
     */
    public function getOutput($kpi_type, $dbb, $obj_id, $obj_id2 = null)
    {
        if ($kpi_type == 0) {
            $hostId = $obj_id;
            $serviceId = $obj_id2;
        } elseif ($kpi_type == 1) {
            $hostId = $this->_meta->getCentreonHostMetaId($dbb);
            $serviceId = $this->_meta->getCentreonServiceMetaId($obj_id, $dbb);
        } elseif ($kpi_type == 2) {
            $hostId = $this->_ba->getCentreonHostBaId($obj_id);
            $serviceId = $this->_ba->getCentreonServiceBaId($obj_id, $hostId);
        } else {
            return "";
        }
        $prefix = $this->_db->getNdoPrefix();
        $query = "SELECT output " .
                "FROM services s " .
                "WHERE s.host_id = " . $hostId . " " .
                "AND s.service_id = " . $serviceId . " ";
        $res = $dbb->query($query);
        while ($row = $res->fetchRow()) {
            return $row['output'];
        }
        return null;
    }

    /*
     *
     */

    public function buildImpactList($db, $element)
    {
        $tab = array();
        $query = "SELECT * FROM mod_bam_impacts ORDER BY code";
        $res = $db->query($query);
        if (!$res->numRows()) {
            throw new Exception('No data found in mod_bam_impacts');
        }
        while ($row = $res->fetchRow()) {
            $code = $row['code'];
            $impactId = $row['id_impact'];
            if ($code == CRITICITY_NULL) {
                $tab[$impactId]['label'] = _('Null');
            } elseif ($code == CRITICITY_WEAK) {
                $tab[$impactId]['label'] = _('Weak');
            } elseif ($code == CRITICITY_MINOR) {
                $tab[$impactId]['label'] = _('Minor');
            } elseif ($code == CRITICITY_MAJOR) {
                $tab[$impactId]['label'] = _('Major');
            } elseif ($code == CRITICITY_CRITICAL) {
                $tab[$impactId]['label'] = _('Critical');
            } elseif ($code == CRITICITY_BLOCKING) {
                $tab[$impactId]['label'] = _('Blocking');
            }
            $tab[$impactId]['color'] = $row['color'];
        }

        foreach ($tab as $c => $value) {
            $element->addOption($tab[$c]['label'], $c, array('style' => 'background-color: ' . $tab[$c]['color'] . '; font-weight: bold'));
        }
    }

    /**
     *
     *
     */
    public function getCriticityLabel($value) {
        if (isset($this->_criticity[$value])) {
            return $this->_criticity[$value];
        }
        return null;
    }

    /**
     *
     *
     */
    public function getCriticityColor($value)
    {
        if (isset($this->_criticityColor[$value])) {
            return $this->_criticityColor[$value];
        }
        return null;
    }

    /**
     *
     */
    public function getCriticityImpact($value)
    {
        if (isset($this->_criticityImpact[$value])) {
            return $this->_criticityImpact[$value];
        }
        return null;
    }

    /**
     * Get criticity code from impact id
     */
    public function getCriticityCode($impactId)
    {
        if (isset($this->_criticityCode[$impactId])) {
            return $this->_criticityCode[$impactId];
        }
        return null;
    }

    /**
     * Get KPI Impact
     *
     */
    public function getKpiImpact($config = array(), $ignore = true)
    {
        $dropValue = 0;

        if (!isset($config['kpi_id'])) {
            throw new Exception(sprintf("Kpi id is not set"));
        }

        if ($ignore === true) {
            if (isset($config['ignore_downtime']) && $config['ignore_downtime'] && $config['downtime']) {
                return $dropValue;
            }
            if (isset($config['ignore_acknowledged']) && $config['ignore_acknowledged'] && $config['acknowledged']) {
                return $dropValue;
            }
        }

        if (!isset($config['config_type']) || !isset($config['current_status'])) {
            return $dropValue;
        }
        if ($config['config_type']) {
            switch ($config['current_status']) {
                case "0" : $dropValue = 0;
                    break;
                case "1" : $dropValue = $config['drop_warning'];
                    break;
                case "2" : $dropValue = $config['drop_critical'];
                    break;
                case "3" : $dropValue = $config['drop_unknown'];
                    break;
            }
        } else {
            switch ($config['current_status']) {
                case "0" : $dropValue = 0;
                    break;
                case "1" : $dropValue = $this->_defaultImpacts[$config['drop_warning_impact_id']];
                    break;
                case "2" : $dropValue = $this->_defaultImpacts[$config['drop_critical_impact_id']];
                    break;
                case "3" : $dropValue = $this->_defaultImpacts[$config['drop_unknown_impact_id']];
                    break;
            }
        }
        return $dropValue;
    }
    
    /**
     * 
     * @param string $listIdKpi
     * @param int $iStatus
     */
    public function setStatusBolleanByKpiId($listIdKpi, $iStatus)
    {
        if (empty($listIdKpi) || !is_numeric($iStatus)) {
            return;
        }
        $query = "UPDATE mod_bam_boolean join mod_bam_kpi ON mod_bam_kpi.boolean_id = mod_bam_boolean.boolean_id  SET mod_bam_boolean.activate = '" . $iStatus. "' WHERE kpi_id  in ($listIdKpi)";
        $this->_db->query($query);
    }
    
    /**
     * 
     * @param type $iBaId
     * @param type $iIdInstance
     * @return boolean
     */
    public function instanceService($iBaId, $iIdInstance)
    {
        if (empty($iBaId) || empty($iIdInstance)) {
            return true;
        }
        
        //check KPI type service
        $sQueryHost = "SELECT host_id FROM mod_bam_kpi WHERE id_ba = '".$this->_db->escape($iBaId)."' AND kpi_type = '0' ";
        $resHost = $this->_db->query($sQueryHost);

        while ($row = $resHost->fetchRow()) {
            $query = "SELECT * FROM ns_host_relation WHERE host_host_id = '".$row['host_id']."' AND nagios_server_id = '".$iIdInstance."'";
            $DBRES = $this->_db->query($query);
            if (!$DBRES->numRows())
                return false;
        }
        
        //check KPI type BA
        $sQueryBa = "SELECT id_indicator_ba FROM mod_bam_kpi WHERE id_ba = '".$this->_db->escape($iBaId)."' AND kpi_type = '2' ";
        $resBa = $this->_db->query($sQueryBa);

        if ($resBa->numRows() > 0) {
            while ($rowBa = $resBa->fetchRow()) {
                $resIndicator = $this->_db->query("SELECT poller_id FROM mod_bam_poller_relations WHERE ba_id = '".$rowBa['id_indicator_ba']."' AND poller_id = '".$iIdInstance."' ");
                if (!$resIndicator->numRows())
                    return false;
            }
        }
        
        //check KPI type boolean
        $sQueryBoolean = "SELECT boolean_id FROM mod_bam_kpi WHERE id_ba = '".$this->_db->escape($iBaId)."' AND kpi_type = '3' ";
        $resBoolean = $this->_db->query($sQueryBoolean);

        if ($resBoolean->numRows() > 0) {
            while ($rowBoolean = $resBoolean->fetchRow()) {
                $booleanId = $rowBoolean['boolean_id'];

                $oBoolean = new CentreonBam_Boolean($this->_db);
                $aBoolean = $oBoolean->getData($booleanId);
                $aResponse = $oBoolean->explodeExpression($aBoolean['expression'], 'object');

                foreach ($aResponse as $objet) {
                    list($hostname, $servicename) = explode(" ", $objet);
                    $hostname = trim($hostname);
                    if (!empty($hostname)) {
                         $resHost = $this->_db->query("SELECT host_host_id FROM ns_host_relation join host on host_host_id = host_id WHERE host_name= '".$hostname."' AND nagios_server_id = '".$iIdInstance."'");
                         if (!$resHost->numRows()) {
                              return false;
                         }
                    }
                }
            }
        }

        return true;
    }
    
    /**
     * 
     * @param type $id_indicator_ba
     * @return array
     */
    public function chainBa($id_indicator_ba)
    {
        $aBa = array();
        $query = "select * from mod_bam_kpi where id_ba = ".$id_indicator_ba;
        $DBRES2 = $this->_db->query($query);

        while ($row = $DBRES2->fetchRow()) {
            if($row['kpi_type'] == "2" || $row['kpi_type'] == "0"){
                $tmp['id_ba'] = $row['id_ba'];
                $tmp['host_id'] = $row['host_id'];
                $tmp['id_indicator_ba'] = $row['id_indicator_ba'];
                $aBa[] = $tmp;
                
                if (!empty($row['id_indicator_ba'])) {
                    $aBa = array_merge($aBa, self::chainBa($row['id_indicator_ba']));
                }

            }
        }
        return $aBa;
    }
    
    /**
    * 
    * @param type $aBaSelected
    * @param type $baIndicator Description
    */
   public function checkKpiTypeBa($aBaSelected, $baIndicator)
   {
       if (!is_array($aBaSelected) || count($aBaSelected) == 0 || empty($baIndicator)) {
           return false;
       }

       //get BA rattached to the indicator ba
       $aBAsIndicator = self::chainBa($baIndicator);
 
       if (count($aBAsIndicator) == 0) {
           foreach ($aBaSelected as $ba) {
                 $res = $this->_db->query("SELECT b.poller_id as poller FROM mod_bam_poller_relations b, nagios_server ns WHERE b.ba_id = '".$ba."' AND ns.id = b.poller_id AND ns.localhost != '1'");
                 if ($res->numRows() > 0) {
                     $row = $res->fetchRow();
                     
                     $resIndicator = $this->_db->query("SELECT b.poller_id as poller FROM mod_bam_poller_relations b, nagios_server ns WHERE b.ba_id = '".$baIndicator."' AND ns.id = b.poller_id AND ns.localhost != '1'");
                     if ($resIndicator->numRows() > 0) {
                         $rowIndicator = $resIndicator->fetchRow();
                         if ($rowIndicator['poller'] != $row['poller'] && !empty($rowIndicator['poller']) && !empty($row['poller']))
                            return false;
                     }
             
                }
            }
       } else {
            foreach ($aBaSelected as $ba) {
                 $res = $this->_db->query("SELECT b.poller_id as poller FROM mod_bam_poller_relations b, nagios_server ns WHERE b.ba_id = '".$ba."' AND ns.id = b.poller_id AND ns.localhost != '1'");
                 //@@todo
                 if ($res->numRows() > 0) {
                     $row = $res->fetchRow();
                     foreach ($aBAsIndicator as $indicator) {
                         if (!empty($indicator['host_id'])) {
                             $query = "SELECT * FROM ns_host_relation WHERE host_host_id = '".$indicator['host_id']."' AND nagios_server_id = '".$row['poller']."'";
                             $DBRES = $this->_db->query($query);
                             if (!$DBRES->numRows())
                                 return false;
                         }
                         if (!empty($indicator['id_indicator_ba'])) {
                             $resIndicator = $this->_db->query("SELECT poller_id FROM mod_bam_poller_relations WHERE ba_id = '".$indicator['id_indicator_ba']."' AND poller_id = '".$row['poller']."' ");
                             if (!$resIndicator->numRows()) {
                                 return false;
                             }
                         }
                     }
                }
            }
       }
       return true;
   }
    
    /**
     * 
     * @param type $aBaSelected
     * @param type $iHost
     * @return boolean
     */
   public function checkKpiTypeService($aBaSelected, $iHost)
   {
       if (!is_array($aBaSelected) || count($aBaSelected) == 0 || empty($iHost)) {
           return false;
       }
       
       foreach ($aBaSelected as $ba) {
           $resultPoller = $this->_db->query("SELECT b.poller_id as poller FROM mod_bam_poller_relations b, nagios_server ns WHERE b.ba_id = '".$ba."' AND ns.id = b.poller_id AND ns.localhost != '1'");

           if ($resultPoller->numRows() > 0) {
               while ($row = $resultPoller->fetchRow()) {
                   $resHost = $this->_db->query("SELECT * FROM ns_host_relation WHERE host_host_id = '".$iHost."' AND nagios_server_id = '".$row['poller']."'");
                   if (!$resHost->numRows())
                       return false;
               }
           } else {
               return true;
           }
       }
       return true;
   }
   
   /**
    * 
    * @param type $aBaSelected
    * @param type $hostGroup
    * @param type $iHostId
    * @return boolean
    */
   public function checkKpiHostGroup($aBaSelected, $hostGroup, $iHostId)
   {
       if (!is_array($aBaSelected) || count($aBaSelected) == 0 || empty($hostGroup) || empty($iHostId)) {
           return false;
       }
       
       foreach ($aBaSelected as $ba) {
           $resultPoller = $this->_db->query("SELECT b.poller_id as poller FROM mod_bam_poller_relations b, nagios_server ns WHERE b.ba_id = '".$ba."' AND ns.id = b.poller_id AND ns.localhost != '1'");

           if ($resultPoller->numRows() > 0) {
               while ($row = $resultPoller->fetchRow()) {
                   $resHost = $this->_db->query("SELECT * FROM ns_host_relation WHERE host_host_id = '".$iHostId."' AND nagios_server_id = '".$row['poller']."'");
                   if (!$resHost->numRows())
                       return false;
               }
           } else {
               return true;
           }
       }
       return true;
   }
   
   /**
    * 
    * @param type $aBaSelected
    * @param type $serviceGroup
    * @param type $iHostId
    * @return boolean
    */
   public function checkKpiServiceGroup($aBaSelected, $serviceGroup, $iHostId)
   {      
       if (!is_array($aBaSelected) || count($aBaSelected) == 0 || empty($serviceGroup) || empty($iHostId)) {
           return false;
       }
       
       foreach ($aBaSelected as $ba) {
           $resultPoller = $this->_db->query("SELECT b.poller_id as poller FROM mod_bam_poller_relations b, nagios_server ns WHERE b.ba_id = '".$ba."' AND ns.id = b.poller_id AND ns.localhost != '1'");
           
           if ($resultPoller->numRows() > 0) {
               while ($row = $resultPoller->fetchRow()) {
                   //Check serviceGroup with service
                   $sQueryService = "SELECT * FROM ns_host_relation WHERE host_host_id = '".$iHostId."' AND nagios_server_id = '".$row['poller']."'";
     
                   $resHost = $this->_db->query($sQueryService);
                   if (!$resHost->numRows())
                       return false;
               }
           }
       }
       return true;
   }
   /**
    * 
    * @param array $aBaSelected
    * @param type $booleanId
    * @return boolean
    */
   public function checkKpiTypeBoolean($aBaSelected, $booleanId)
   {
       if (!is_array($aBaSelected) || count($aBaSelected) == 0 || empty($booleanId)) {
           return false;
       }
       
       $oBoolean = new CentreonBam_Boolean($this->_db);
       $aBoolean = $oBoolean->getData($booleanId);
       $aResponse = $oBoolean->explodeExpression($aBoolean['expression'], 'object');
       
       foreach ($aBaSelected as $ba) {
           $resultPoller = $this->_db->query("SELECT b.poller_id as poller FROM mod_bam_poller_relations b, nagios_server ns WHERE b.ba_id = '".$ba."' AND ns.id = b.poller_id AND ns.localhost != '1'");

           if ($resultPoller->numRows() > 0) {
               while ($row = $resultPoller->fetchRow()) {
                   foreach ($aResponse as $objet) {
 
                       list($hostname, $servicename) = explode(" ", $objet);
                       $hostname = trim($hostname);
                       if (!empty($hostname)) {
                            $resHost = $this->_db->query("SELECT host_host_id FROM ns_host_relation join host on host_host_id = host_id WHERE host_name= '".$hostname."' AND nagios_server_id = '".$row['poller']."'");
                            if (!$resHost->numRows()) {
                                 return false;
                            }
                       }
                   }
               }
           }
       }
       return true;
   }
}

?>
