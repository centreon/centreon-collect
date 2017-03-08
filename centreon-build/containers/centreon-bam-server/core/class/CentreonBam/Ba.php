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

/*
 *  Checks if Group already exists
 *  if it exists, function returns false
 *  otherwise, it returns true
 */

/*
 * Class that contains all methods for managing BA
 */
class CentreonBam_Ba
{

    protected $_db;
    protected $_dbNdo;
    protected $_form;
    protected $_cacheName;
    protected $_cacheDescription;
    protected $_cacheIcon;
    protected $baParams;
    protected $cacheBaListByService;

    /*
     *  Constructor
     */
    public function __construct($db, $form = null, $dbNdo)
    {
        $this->_db    = $db;
        $this->_form  = $form;
        $this->_dbNdo = $dbNdo;
        $this->_cacheName = array();
        $this->_cacheDescription = array();
        $this->_cacheIcon = array();
        $this->_cacheId = array();
        $this->baParams = array();
        $this->_createCache();
    }
    
    /**
     * 
     * @param array $newBaParams
     */
    public function setBaParams($newBaParams)
    {
        $this->baParams = $newBaParams;
    }
    
    /**
     * 
     * @return array
     */
    public function getBaParams()
    {
        return $this->baParams;
    }
    
    /**
     * 
     * @param string $key
     */
    private function getBaConfigurationDatas($key = '')
    {
        $datasToBeReturned = array();
        
        if (count($this->baParams) > 0) {
            if (empty($key)) {
                $datasToBeReturned = $this->baParams;
            } else {
                if (isset($this->baParams[$key])) {
                    $datasToBeReturned = $this->baParams[$key];
                }
            }
        } else {
            if (empty($key)) {
                $datasToBeReturned = $this->_form->getSubmitValues();
            } else {
                $datasToBeReturned = $this->_form->getSubmitValue($key);
            }
        }
        
        return $datasToBeReturned;
    }

    /*
     *
     */
    protected function _createCache()
    {
        $rq = "SELECT ba_id, name, description, icon_id FROM mod_bam";
        $res = $this->_db->query($rq);
        while ($row = $res->fetchRow()) {
            $this->_cacheName[$row['ba_id']] = $row['name'];
            $this->_cacheDescription[$row['ba_id']] = $row['description'];
            $this->_cacheIcon[$row['ba_id']] = $row['icon_id'];
            $this->_cacheId[$row['name']] = $row['ba_id'];
        }
        $this->buildBaListByServiceDescription();
    }
    
    /**
     * Updates contact groups
     * @param integer $baId
     */
    private function updateCg($baId)
    {
        $rq = "DELETE FROM mod_bam_cg_relation ";
        $rq .= "WHERE id_ba = '" . $baId . "'";
        $this->_db->query($rq);
        $ret = array();
        $ret = $this->getBaConfigurationDatas("bam_contact");
        $str = "INSERT INTO `mod_bam_cg_relation` (id_ba, id_cg) VALUES ";
        for ($i = 0; $i < count($ret); $i++) {
            if ($i) {
                $str .= ", ";
            }
            $str .= "('" . $baId . "', '" . $ret[$i] . "')";
        }
        if ($i) {
            $this->_db->query($str);
        }
    }
    
    /**
     * Updates escalation
     * @param type $baId
     */
    private function updateEsc($baId)
    {
        $rq = "DELETE FROM mod_bam_escal_relation ";
        $rq .= "WHERE id_ba = '" . $baId . "'";
        $this->_db->query($rq);
        $ret = array();
        $ret = $this->getBaConfigurationDatas("bam_esc");
        $str = "INSERT INTO `mod_bam_escal_relation` (id_ba, id_esc) VALUES ";
        for ($i = 0; $i < count($ret); $i++) {
            if ($i) {
                $str .= ", ";
            }
            $str .= "('" . $baId . "', '" . $ret[$i] . "')";
        }
        if ($i) {
            $this->_db->query($str);
        }
    }

    /**
     * Update reporting time periods that will be used by BI
     *
     * @param int $baId
     * @param int $defaultReportingPeriod
     */
    private function updateBiReportingPeriods($baId, $defaultReportingPeriod)
    {
        $baId = $this->_db->escape($baId);

        /* delete old relations first */
        $sql = "DELETE FROM mod_bam_relations_ba_timeperiods WHERE ba_id = {$baId}";
        $this->_db->query($sql);
        $list = $this->getBaConfigurationDatas("reporting_timeperiods");
        
        if (is_array($list) && count($list)) {
            $sql = "INSERT INTO mod_bam_relations_ba_timeperiods (ba_id, tp_id) VALUES ";
            $tmpSql = "";
            foreach ($list as $tpId) {
                /* don't insert if tp id is used as default already */
                if (!is_null($defaultReportingPeriod) && $tpId == $defaultReportingPeriod) {
                    continue;
                }
                if ($tmpSql) {
                    $tmpSql .= ", ";
                }
                $tmpSql .= " ({$baId}, {$tpId}) ";
            }
            $this->_db->query($sql . $tmpSql);
        }
    }
    
    /**
     * Updates Group <-> BA relation
     * @param int $baId
     * @return type
     */
    private function updateBA_Group_List($baId = null)
    {
        if (!isset($baId)) {
            return null;
        }

        $rq = "DELETE FROM `mod_bam_bagroup_ba_relation` ";
        $rq .= "WHERE id_ba = '" . $baId . "'";
        $this->_db->query($rq);

        $tab = array();
        $tab = $this->getBaConfigurationDatas("ba_group_list");
        
        for ($i = 0; $i < count($tab); $i++) {
            $rq = "INSERT INTO `mod_bam_bagroup_ba_relation` ";
            $rq .= "(id_ba, id_ba_group) ";
            $rq .= "VALUES ";
            $rq .= "('" . $baId . "', '" . $tab[$i] . "')";
            $this->_db->query($rq);
            notifyAclConfChangedByBv($this->_db, $tab[$i]);
        }
    }
    
    /**
     * Update the additional poller if set
     *
     * @param int $baId The business activity ID
     */
    private function updatePollerDep($baId)
    {
        /* Clean up old relation */
        $this->_db->query("DELETE FROM mod_bam_poller_relations WHERE ba_id = " . $baId);
        
        /* Insert new relation */
        $values = $this->getBaConfigurationDatas();
        $this->_db->query("INSERT INTO mod_bam_poller_relations(ba_id, poller_id)
            SELECT " . $baId . ", id FROM nagios_server WHERE localhost = '1'");
        if ($values['additional_poller'] != 0) {
            $this->_db->query("INSERT INTO mod_bam_poller_relations (ba_id, poller_id) VALUES (" . $baId . ", " . $values['additional_poller'] . ")");
            $hostId = getCentreonBaHostId($this->_db, $values['additional_poller']);
        }
        
        /* Get localhost poller */
        $query = "SELECT id FROM nagios_server WHERE localhost = '1'";
        $res = $this->_db->query($query);
        $row = $res->fetchRow();
        
        /* Get virtual host for central */
        $centralId = getCentreonBaHostId($this->_db, $row['id']);
        
        /* Update virtual host / service relation */
        try {
            $svcId = getCentreonBaServiceId($this->_db, "ba_" . $baId);
            $query = "DELETE FROM host_service_relation WHERE service_service_id = " . $svcId;
            $this->_db->query($query);
            $query = "INSERT INTO host_service_relation (host_host_id, service_service_id)
                    VALUES (" . $centralId . ", " . $svcId . ")";
            $this->_db->query($query);
            if ($values['additional_poller'] != 0) {
                $query = "INSERT INTO host_service_relation (host_host_id, service_service_id)
                    VALUES (" . $values['additional_poller'] . ", " . $svcId . ")";
                $this->_db->query($query);
            }
        } catch (Exception $e) {
        }
    }
    
    /**
     * Tests if BA name is already used
     * Returns true if the name is available
     * False, if is already used
     * @param type $name
     * @param type $baId
     * @return boolean
     */
    public function testBAExistence($name, $baId = null)
    {
        $query = "SELECT * FROM `mod_bam` WHERE name = '" . $this->_db->escape($name) . "'";
        if (isset($baId)) {
            $query .= " AND ba_id != '" . $baId . "'";
        }
        $res = $this->_db->query($query);
        if ($res->numRows()) {
            return false;
        }
        return true;
    }
    
    /**
     *  Inserts BA
     * @return int
     */
    public function insertBAInDB()
    {
        $baName = '';
        if (count($this->baParams) > 0) {
            $baName .= $this->baParams['ba_name'];
        } else {
            $baName .= $_POST['ba_name'];
        }
        
        if (!$this->testBAExistence($baName)) {
            return 0;
        }

        $tab = array();
        $tab = $this->getBaConfigurationDatas();
        $query = "INSERT INTO `mod_bam` " .
            "(`name`, `description`, `level_w`, `level_c`, `sla_month_percent_warn`, `sla_month_percent_crit`, 
            `sla_month_duration_warn`, `sla_month_duration_crit`,
            `id_notification_period`, `id_reporting_period`, `notification_interval`, `notification_options`, 
            `notifications_enabled`, `current_level`, `comment`, `activate`, `inherit_kpi_downtimes`, `icon_id`, 
	        `event_handler_enabled`, `event_handler_command`, `event_handler_args`, 
            `graph_style`) VALUES (";

        $query .= isset($tab["ba_name"]) && $tab["ba_name"] != null ? "'" . $this->_db->escape($tab["ba_name"]) . "', " : "NULL, ";
        $query .= isset($tab["ba_desc"]) && $tab["ba_desc"] != null ? "'" . $this->_db->escape($tab["ba_desc"]) . "', " : "NULL, ";
        $query .= isset($tab["ba_warning"]) && $tab["ba_warning"] != null ? "'" . $tab["ba_warning"] . "', " : "NULL, ";
        $query .= isset($tab["ba_critical"]) && $tab["ba_critical"] != null ? "'" . $tab["ba_critical"] . "', " : "NULL, ";
        $query .= isset($tab["sla_month_percent_warn"]) && $tab["sla_month_percent_warn"] != null ? "'" . $tab['sla_month_percent_warn'] . "', " : "NULL, ";
        $query .= isset($tab["sla_month_percent_crit"]) && $tab["sla_month_percent_crit"] != null ? "'" . $tab['sla_month_percent_crit'] . "', " : "NULL, ";
        $query .= isset($tab["sla_month_duration_warn"]) && $tab["sla_month_duration_warn"] != null ? "'" . $tab['sla_month_duration_warn'] . "', " : "NULL, ";
        $query .= isset($tab["sla_month_duration_crit"]) && $tab["sla_month_duration_crit"] != null ? "'" . $tab['sla_month_duration_crit'] . "', " : "NULL, ";
        $query .= isset($tab["id_notif_period"]) && $tab["id_notif_period"] != null ? "'" . $tab["id_notif_period"] . "', " : "NULL, ";
        $query .= isset($tab["id_reporting_period"]) && $tab["id_reporting_period"] != null ? "'" . $tab["id_reporting_period"] . "', " : "NULL, ";
        $query .= isset($tab["notif_interval"]) && $tab["notif_interval"] != null ? "'" . $tab["notif_interval"] . "', " : "NULL, ";
        $query .= isset($tab["notifOpts"]) && $tab["notifOpts"] != null ? "'" . implode(",", array_keys($tab["notifOpts"])) . "', " : "NULL, ";
        $query .= isset($tab["notifications_enabled"]["notifications_enabled"]) && $tab["notifications_enabled"]["notifications_enabled"] != NULL ? "'" . $tab["notifications_enabled"]["notifications_enabled"] . "', " : "NULL, ";
        $query .= "0, ";
        $query .= isset($tab["bam_comment"]) && $tab["bam_comment"] != null ? "'" . $this->_db->escape($tab["bam_comment"]) . "', " : "NULL, ";
        $query .= isset($tab["bam_activate"]["bam_activate"]) && $tab["bam_activate"]["bam_activate"] != null ? "'" . $tab["bam_activate"]["bam_activate"] . "', " : "NULL, ";
        $query .= isset($tab["inherit_kpi_downtimes"]["inherit_kpi_downtimes"]) && $tab["inherit_kpi_downtimes"]["inherit_kpi_downtimes"] != null ? "'" . $tab["inherit_kpi_downtimes"]["inherit_kpi_downtimes"] . "', " : "NULL, ";
        $query .= isset($tab["icon"]) && $tab["icon"] != null ? "'" . $tab["icon"] . "', " : "NULL, ";

        $query .= isset($tab["event_handler_enabled"]["event_handler_enabled"]) && $tab["event_handler_enabled"]["event_handler_enabled"] != NULL ? "'" . $tab["event_handler_enabled"]["event_handler_enabled"] . "', " : "NULL, ";
        $query .= isset($tab["event_handler_command"]) && $tab["event_handler_command"] != null ? "'" . $tab["event_handler_command"] . "', " : "NULL, ";
        $query .= isset($tab["event_handler_args"]) && $tab["event_handler_args"] != null ? "'" . $this->_db->escape($tab["event_handler_args"]) . "', " : "NULL, ";
 
        $query .= isset($tab["graph_style"]) && $tab["graph_style"] != null ? "'" . $tab["graph_style"] . "'" : "NULL ";
        $query .= ")";
        $this->_db->query($query);

        $query2 = "SELECT MAX(ba_id) FROM `mod_bam` LIMIT 1";
        $res = $this->_db->query($query2);
        if (!$res->numRows()) {
            return null;
        }
        $row = $res->fetchRow();
        $this->updateCg($row['MAX(ba_id)']);
        $this->updateEsc($row['MAX(ba_id)']);
        
        $this->updateBA_Group_List($row['MAX(ba_id)']);
        $this->updateBiReportingPeriods($row['MAX(ba_id)'], $tab['id_reporting_period']);
        $this->updatePollerDep($row['MAX(ba_id)']);
        return ($row['MAX(ba_id)']);
    }

    /**
     * Update a business activity (BA) in database
     *
     * @param int $baId The business activity ID
     */
    public function updateBA($baId)
    {
        $tab = array();
        $tab = $this->getBaConfigurationDatas();

        if (!$this->testBAExistence($_POST['ba_name'], $baId)) {
            return 0;
        }

        $query = "UPDATE `mod_bam` SET ";
        $query .= "`name` = ";
        $query .= isset($tab["ba_name"]) && $tab["ba_name"] != null ? "'" .  $this->_db->escape($tab["ba_name"]) . "', " : "NULL, ";
        $query .= "`description` = ";
        $query .= isset($tab["ba_desc"]) && $tab["ba_desc"] != null ? "'" . $this->_db->escape($tab["ba_desc"]) . "', " : "NULL, ";
        $query .= "`level_w` = ";
        $query .= isset($tab["ba_warning"]) && $tab["ba_warning"] != null ? "'" . $tab["ba_warning"] . "', " : "NULL, ";
        $query .= "`level_c` = ";
        $query .= isset($tab["ba_critical"]) && $tab["ba_critical"] != null ? "'" . $tab["ba_critical"] . "', " : "NULL, ";
        $query .= "`sla_month_percent_warn` = ";
        $query .= isset($tab["sla_month_percent_warn"]) && $tab["sla_month_percent_warn"] != null ? "'" . $tab["sla_month_percent_warn"] . "', " : "NULL, ";
        $query .= "`sla_month_percent_crit` = ";
        $query .= isset($tab["sla_month_percent_crit"]) && $tab["sla_month_percent_crit"] != null ? "'" . $tab["sla_month_percent_crit"] . "', " : "NULL, ";
        $query .= "`sla_month_duration_warn` = ";
        $query .= isset($tab["sla_month_duration_warn"]) && $tab["sla_month_duration_warn"] != null ? "'" . $tab["sla_month_duration_warn"] . "', " : "NULL, ";
        $query .= "`sla_month_duration_crit` = ";
        $query .= isset($tab["sla_month_duration_crit"]) && $tab["sla_month_duration_crit"] != null ? "'" . $tab["sla_month_duration_crit"] . "', " : "NULL, ";
        $query .= "`id_notification_period` = ";
        $query .= isset($tab["id_notif_period"]) && $tab["id_notif_period"] != null ? "'" . $tab["id_notif_period"] . "', " : "NULL, ";
        $query .= "`id_reporting_period` = ";
        $query .= isset($tab["id_reporting_period"]) && $tab["id_reporting_period"] != null ? "'" . $tab["id_reporting_period"] . "', " : "NULL, ";
        $query .= "`notification_interval` = ";
        $query .= isset($tab["notif_interval"]) && $tab["notif_interval"] != null ? "'" . $tab["notif_interval"] . "', " : "NULL, ";
        $query .= "`notification_options` = ";
        $query .= isset($tab["notifOpts"]) && $tab["notifOpts"] != null ? "'" . implode(",", array_keys($tab["notifOpts"])) . "', " : "NULL, ";
        $query .= "`notifications_enabled` = ";
        $query .= isset($tab["notifications_enabled"]["notifications_enabled"]) && $tab["notifications_enabled"]["notifications_enabled"] != null ? "'" . $tab["notifications_enabled"]["notifications_enabled"] . "', " : "NULL, ";
        $query .= "`comment` = ";
        $query .= isset($tab["bam_comment"]) && $tab["bam_comment"] != null ? "'" . $this->_db->escape($tab["bam_comment"]) . "', " : "NULL, ";
        $query .= "`activate` = ";
        $query .= isset($tab["bam_activate"]["bam_activate"]) && $tab["bam_activate"]["bam_activate"] != null ? "'" . $tab["bam_activate"]["bam_activate"] . "', " : "NULL, ";
        
        $query .= "`inherit_kpi_downtimes` = ";
        $query .= isset($tab["inherit_kpi_downtimes"]["inherit_kpi_downtimes"]) && $tab["inherit_kpi_downtimes"]["inherit_kpi_downtimes"] != null ? "'" . $tab["inherit_kpi_downtimes"]["inherit_kpi_downtimes"] . "', " : "NULL, ";
        
        $query .= "`icon_id` = ";
        $query .= isset($tab["icon"]) && $tab["icon"] != null ? "'" . $tab["icon"] . "', " : "NULL, ";

        $query .= "`event_handler_enabled` = ";
        $query .= isset($tab["event_handler_enabled"]["event_handler_enabled"]) && $tab["event_handler_enabled"]["event_handler_enabled"] != null ? "'" . $tab["event_handler_enabled"]["event_handler_enabled"] . "', " : "NULL, ";
        $query .= "`event_handler_command` = ";
        $query .= isset($tab["event_handler_command"]) && $tab["event_handler_command"] != null ? "'" . $tab["event_handler_command"] . "', " : "NULL, ";
        $query .= "`event_handler_args` = ";
        $query .= isset($tab["event_handler_args"]) && $tab["event_handler_args"] != null ? "'" . $this->_db->escape($tab["event_handler_args"]) . "', " : "NULL, ";

        $query .= "`graph_style` = ";
        $query .= isset($tab["graph_style"]) && $tab["graph_style"] != null ? "'" . $tab["graph_style"] . "', " : "NULL ";
        $query .= "WHERE ba_id = '" . $baId . "'";

        $this->_db->query($query);
        $this->updateCg($baId);
        $this->updateEsc($baId);
        $this->updateBA_Group_List($baId);
        $this->updateBiReportingPeriods($baId, $tab['id_reporting_period']);
        $this->updatePollerDep($baId);
    }
    
    /**
     * Sets status (enable/disable) of a BA view
     * @param type $baId
     * @param type $status
     * @param type $multiSelect
     */
    public function setActivate($baId, $status, $multiSelect = null)
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
            $list = "'" . $baId . "'";
        }
        $query = "UPDATE `mod_bam` " .
                "SET activate = '" . $status . "' " .
                "WHERE ba_id IN ($list)";
        $this->_db->query($query);
    }
    
    /**
     * Deletes a BA from database
     * @param type $baId
     * @param type $multiSelect
     */
    public function deleteBA($baId, $multiSelect = NULL)
    {
        $list = "";
        $listSvc = array();
        if (isset($multiSelect)) {
            foreach ($multiSelect as $key => $value) {
                if ($list != "") {
                    $list .= ", ";
                }
                $list .= "'" . $key . "'";
                $listSvc[] = getCentreonBaServiceId($this->_db, "ba_" . $key);
            }
            if ($list == "") {
                $list = "''";
            }
        } else {
            $list = "'" . $baId . "'";
            $listSvc[] = getCentreonBaServiceId($this->_db, "ba_" . $baId);
        }
        $query = "DELETE b, d FROM `mod_bam` b LEFT JOIN dependency d on d.dep_id = b.dependency_dep_id WHERE b.ba_id IN ($list) ";
        $this->_db->query($query);
        
        /* Delete virtual host service relation */
        if (count($listSvc) > 0) {
            $query = "DELETE FROM host_service_relation WHERE service_service_id IN (" . join(', ', $listSvc) . ")";
            $this->_db->query($query);
            $query = "DELETE FROM service WHERE service_id IN (" . join(', ', $listSvc) . ")";
            $this->_db->query($query);
        }
    }
    
    /**
     * Returns Name of Business Activity from ID
     * @param int $baId
     * @return mixed
     */
    public function getBA_Name($baId)
    {
        if (isset($this->_cacheName[$baId])) {
            return $this->_cacheName[$baId];
        }
        return null;
    }
    
    /**
     * returns Description of BA from ID
     * @param int $baId
     * @return mixed
     */
    public function getBA_Desc($baId)
    {
        if (isset($this->_cacheDescription[$baId])) {
            return $this->_cacheDescription[$baId];
        }
        return null;
    }
    
    /**
     * returns id of BA from name
     * @param string $baName
     * @return int
     */
    public function getBA_id($baName)
    {
        if (isset($this->_cacheId[$baName])) {
            return $this->_cacheId[$baName];
        }
        $query = "SELECT ba_id FROM `mod_bam` WHERE name = '" . $baName . "' LIMIT 1";
        $res = $this->_db->query($query);
        if (!$res->numRows()) {
            return null;
        }
        $row = $res->fetchRow();
        return ($row['ba_id']);
    }
    
    /**
     * Returns list of all active BA
     * @param type $aclFilter
     * @return array
     */
    public function getBA_list($aclFilter = '')
    {
        $tab = array();
        $query = "SELECT ba_id, name FROM `mod_bam` WHERE activate = '1' $aclFilter ORDER BY name";
        $res = $this->_db->query($query);
        if (!$res->numRows()) {
            return $tab;
        }
        while ($row = $res->fetchRow()) {
            $tab[$row['ba_id']] = $row['name'];
        }
        return ($tab);
    }
    
    /**
     * Literally duplicates the ba with sql query
     * @param type $originalId
     * @param type $nb
     * @return type
     */
    private function duplicate_ba($originalId, $nb)
    {
        /* BAM duplication process */
        $query = "INSERT INTO `mod_bam` (name, description, level_w, level_c,
                sla_month_percent_warn, sla_month_percent_crit,
                sla_month_duration_warn, sla_month_duration_crit,
				id_notification_period, id_reporting_period, notification_interval,
				notification_options, notifications_enabled, max_check_attempts,
				normal_check_interval, retry_check_interval, calculate, downtime,
				dependency_dep_id, graph_id, activate, comment, icon_id, graph_style)
				(SELECT name, description, level_w, level_c,
                sla_month_percent_warn, sla_month_percent_crit,
                sla_month_duration_warn, sla_month_duration_crit,
				id_notification_period, id_reporting_period, notification_interval,
				notification_options, notifications_enabled, max_check_attempts,
				normal_check_interval, retry_check_interval, calculate, downtime,
				dependency_dep_id, graph_id, activate, comment, icon_id, graph_style FROM `mod_bam` WHERE ba_id = '" . $originalId . "')";
        $this->_db->query($query);

        $query2 = "SELECT MAX(ba_id) FROM `mod_bam`";
        $res = $this->_db->query($query2);
        $row = $res->fetchRow();
        $newId = $row['MAX(ba_id)'];
        
        $query3 = "SELECT name FROM `mod_bam` WHERE ba_id = '" .$newId. "'";
        $res2 = $this->_db->query($query3);
        $row2 = $res2->fetchRow();
        $name = $row2['name'];
        
        /* ba group relation duplicatin process */
        $query5 = "SELECT id_ba_group FROM `mod_bam_bagroup_ba_relation` WHERE id_ba = '" . $originalId . "'";
        $res5 = $this->_db->query($query5);
        $str = "";
        while ($row5 = $res5->fetchRow()) {
            if ($str != "") {
                $str .= ",";
            }
            $str .= "('" . $newId . "', '" . $row5['id_ba_group'] . "')";
        }
        if ($str != "") {
            $query5 = "INSERT INTO `mod_bam_bagroup_ba_relation` (id_ba, id_ba_group) VALUES " . $str;
            $this->_db->query($query5);
        }

        /* contact group relation duplicatin process */
        $query6 = "SELECT id_cg FROM `mod_bam_cg_relation` WHERE id_ba = '" . $originalId . "'";
        $res6 = $this->_db->query($query6);
        $str = "";
        while ($row6 = $res6->fetchRow()) {
            if ($str != "") {
                $str .= ",";
            }
            $str .= "('" . $newId . "', '" . $row6['id_cg'] . "')";
        }
        if ($str != "") {
            $query6 = "INSERT INTO `mod_bam_cg_relation` (id_ba, id_cg) VALUES " . $str;
            $this->_db->query($query6);
        }


        /* dependency child relation duplicatin process */
        $query7 = "SELECT id_dep FROM `mod_bam_dep_child_relation` WHERE id_ba = '" . $originalId . "'";
        $res7 = $this->_db->query($query7);
        $str = "";
        while ($row7 = $res7->fetchRow()) {
            if ($str != "") {
                $str .= ",";
            }
            $str .= "('" . $newId . "', '" . $row7['id_dep'] . "')";
        }
        if ($str != "") {
            $query7 = "INSERT INTO `mod_bam_dep_child_relation` (id_ba, id_dep) VALUES " . $str;
            $this->_db->query($query7);
        }

        /* dependency parent relation duplicatin process */
        $query7 = "SELECT id_dep FROM `mod_bam_dep_parent_relation` WHERE id_ba = '" . $originalId . "'";
        $res7 = $this->_db->query($query7);
        $str = "";
        while ($row7 = $res7->fetchRow()) {
            if ($str != "") {
                $str .= ",";
            }
            $str .= "('" . $newId . "', '" . $row7['id_dep'] . "')";
        }
        if ($str != "") {
            $query7 = "INSERT INTO `mod_bam_dep_parent_relation` (id_ba, id_dep) VALUES " . $str;
            $this->_db->query($query7);
        }

        /* escalation relation duplicatin process */
        $query7 = "SELECT id_esc FROM `mod_bam_escal_relation` WHERE id_ba = '" . $originalId . "'";
        $res7 = $this->_db->query($query7);
        $str = "";
        while ($row7 = $res7->fetchRow()) {
            if ($str != "") {
                $str .= ",";
            }
            $str .= "('" . $newId . "', '" . $row7['id_esc'] . "')";
        }
        if ($str != "") {
            $query7 = "INSERT INTO `mod_bam_escal_relation` (id_ba, id_esc) VALUES " . $str;
            $this->_db->query($query7);
        }

        /* kpi duplication process */
        $query8 = "SELECT * FROM `mod_bam_kpi` WHERE id_ba = '" . $originalId . "'";
        $res8 = $this->_db->query($query8);
        $str = "";
        while ($row8 = $res8->fetchRow()) {
            if ($str != "") {
                $str .= ",";
            }
            $row8['host_id'] = isset($row8['host_id']) && ($row8['host_id'] != "") ? $row8['host_id'] : "NULL";
            $row8['service_id'] = isset($row8['service_id']) && ($row8['service_id'] != "") ? $row8['service_id'] : "NULL";
            $row8['meta_id'] = isset($row8['meta_id']) && ($row8['meta_id'] != "") ? $row8['meta_id'] : "NULL";
            $row8['boolean_id'] = isset($row8['boolean_id']) && ($row8['boolean_id'] != "") ? $row8['boolean_id'] : "NULL";
            $row8['id_indicator_ba'] = isset($row8['id_indicator_ba']) && ($row8['id_indicator_ba'] != "") ? $row8['id_indicator_ba'] : "NULL";
            
            if (!isset($row8['drop_warning']) || $row8['drop_warning'] == "") {
                $row8['drop_warning'] = "NULL";
            }
            if (!isset($row8['drop_critical']) || $row8['drop_critical'] == "") {
                $row8['drop_critical'] = "NULL";
            }
            if (!isset($row8['drop_unknown']) || $row8['drop_unknown'] == "") {
                $row8['drop_unknown'] = "NULL";
            }
            if (!isset($row8['drop_warning_impact_id']) || $row8['drop_warning_impact_id'] == "") {
                $row8['drop_warning_impact_id'] = "NULL";
            }
            if (!isset($row8['drop_critical_impact_id']) || $row8['drop_critical_impact_id'] == "") {
                $row8['drop_critical_impact_id'] = "NULL";
            }
            if (!isset($row8['drop_unknown_impact_id']) || $row8['drop_unknown_impact_id'] == "") {
                $row8['drop_unknown_impact_id'] = "NULL";
            }

            $str .= "(" . $newId . ", '" . $row8['kpi_type'] . "', " . $row8['host_id'] . ", " . $row8['service_id'] . ", " . $row8['id_indicator_ba'] . ", " . $row8['meta_id'] . ", " . $row8['boolean_id'] . ", " . 
                    "'" . $this->_db->escape($row8['comment']) . "', '" . $row8['config_type'] . "', " . $row8['drop_warning'] . ", " . $row8['drop_warning_impact_id'] . ", " . $row8['drop_critical'] . ", " . $row8['drop_critical_impact_id'] . ", " . $row8['drop_unknown'] . ", " . $row8['drop_unknown_impact_id'] . ", '" . $row8['activate'] . "')";
        }
        if ($str != "") {
            $query8 = "INSERT INTO `mod_bam_kpi` (id_ba, kpi_type, host_id, service_id, id_indicator_ba, meta_id, boolean_id, `comment`, config_type, drop_warning, drop_warning_impact_id, drop_critical, drop_critical_impact_id, drop_unknown, drop_unknown_impact_id, activate) VALUES " . $str;
            $this->_db->query($query8);
        }

        /* extra reporting timeperiods */
        $query9 = "INSERT INTO mod_bam_relations_ba_timeperiods (ba_id, tp_id) 
            (SELECT {$newId}, tp_id FROM mod_bam_relations_ba_timeperiods WHERE ba_id = {$originalId})";
        $this->_db->query($query9);
        
        
        /* Pollers */
        $query10 = "INSERT INTO mod_bam_poller_relations (ba_id, poller_id) 
            (SELECT {$newId}, poller_id FROM mod_bam_poller_relations WHERE ba_id = {$originalId})";
        $this->_db->query($query10);
        
        //Rennomage BA
        $insert= false;
        while (!$insert) {
            $sNewName =  $name . "_" . $nb;
            $query3 = "SELECT name FROM `mod_bam` WHERE name = '" .$sNewName. "'";
            $res = $this->_db->query($query3);
            if (!$res->numRows()) {
                $insert = true;
                $query4 = "UPDATE `mod_bam` SET name = '" .$sNewName."' WHERE ba_id = '" . $newId . "'";
                $this->_db->query($query4);
            } else {
                $nb = $nb + 1;
            }
            
        }

        return $newId;
    }
    
    /**
     * Duplicate a ba group
     * @param type $multiSelect
     * @return type
     */
    public function duplicate($multiSelect = null)
    {
        if (!isset($multiSelect)) {
            return null;
        }
        foreach ($multiSelect as $key => $value) {
            $nb_dup = 0;
            if (isset($_POST['dup_' . $key]) && is_numeric($_POST['dup_' . $key])) {
                $nb_dup = $_POST['dup_' . $key];
                for ($i = 0; $i < $nb_dup; $i++) {
                    $newId = $this->duplicate_ba($key, $i + 1);
                    // insert new service id
                    if (function_exists('getCentreonBaServiceId')) {
                        getCentreonBaServiceId($this->_db, 'ba_' . $newId);
                    }
                }
            }
        }
    }
    
    /**
     * get Nagios icon path
     * @param type $dbndo
     * @param type $baId
     * @param type $root
     * @return type
     */
    public function getIconPath($dbndo, $baId, $root = "")
    {
        $name = $this->getBA_Name($baId);
        $query = "SELECT icon_image FROM " . $this->_db->getNdoPrefix() . "services WHERE description = '" . $name . "' LIMIT 1";
        $res = $dbndo->query($query);
        if ($res->numRows()) {
            $row = $res->fetchRow();
            if (isset($row['icon_image']) && $row['icon_image'] != "") {
                return ($root . $row['icon_image']);
            }
            return null;
        }
        return null;
    }
    
    /**
     * get Nagios icon id
     * @param int $baId
     * @return mixed
     */
    public function getIconId($baId)
    {
        if (isset($this->_cacheIcon[$baId])) {
            return $this->_cacheIcon[$baId];
        }
        return null;
    }
    
    /**
     * 
     * @staticvar type $hostId
     * @param type $baId
     * @return int
     */
    public function getCentreonHostBaId($baId)
    {
        static $hostId = null;
        
        $idPoller = null;    
        $queryPoller = "SELECT b.poller_id as poller, localhost, id, ba_id FROM mod_bam_poller_relations b right join nagios_server ns on ns.id = b.poller_id AND b.ba_id = '".$this->_db->escape($baId)."'";
        $resPoller = $this->_db->query($queryPoller);        
        if ($resPoller->numRows() > 0) {
            while($rowPoller = $resPoller->fetchRow()) {
                if ($rowPoller['localhost'] == '1') {
                    $idCentral = $rowPoller['id'];
                }
                if ($rowPoller['ba_id'] && $rowPoller['localhost'] == '0') {
                    $idPoller = $rowPoller['poller'];
                }
            }
        }
        
        if (is_null($idPoller) && isset($idCentral)) {
            $idPoller = $idCentral;
        }
        
        if (!isset($hostId)) {
            $sStr = "_Module_BAM_".$idPoller;

            $query = "SELECT host_id FROM hosts WHERE name like '".$sStr."%' AND enabled = 1 LIMIT 1";
            $res = $this->_dbNdo->query($query);
            if ($res->numRows()) {
                $row = $res->fetchRow();
                $hostId = $row['host_id'];
            } else {
                $hostId = 0;
            }
        }
        return $hostId;
    }
    
    
    
    private function buildBaListByServiceDescription()
    {
        $query = "SELECT s.description FROM services s WHERE s.description LIKE 'ba\_%' AND s.enabled = 1";
        $res = $this->_dbNdo->query($query);
        if ($res->numRows()) {
            while ($row = $res->fetchRow()) {
                $this->cacheBaListByService = $row['description'];
            }
        }
    }

    /**
     * Get Centreon Service_Id by ba_id
     *
     * @param int $baId
     * @param CentreonBam_Db $dbc
     * @return int
     */
    public function getCentreonServiceBaId($baId, $hostId)
    {
        if (empty($hostId)) {
            return 0;
        }
        $service_id = 0;
        
        $query = "SELECT service_id,
                          description
                   FROM services s, hosts h
                   WHERE s.host_id = h.host_id
                   AND s.description LIKE 'ba_".$baId."'
                   AND h.host_id = " . $hostId;
        $res = $this->_dbNdo->query($query);
        if ($res->numRows()) {
            while ($row = $res->fetchRow()) {
                $service_id = $row['service_id'];
            }
        }
        return $service_id;
    }
    
    /**
     * 
     * @param int $baId
     * @return array
     */
    public function getCentreonConfigurationHostServiceIdByBaId($baId)
    {
        $hostService = array();
        
        $query = "SELECT s.service_id, hsr.host_host_id "
            . "FROM host_service_relation hsr, service s "
            ." WHERE s.service_description = 'ba_$baId' "
            . "AND hsr.service_service_id = s.service_id";
        $res = $this->_db->query($query);
        if ($res->numRows()) {
            while ($row = $res->fetchRow()) {
                $hostService['host'] = $row['host_host_id'];
                $hostService['service'] = $row['service_id'];
            }
        }
        
        return $hostService;
    }
    
    /**
     * 
     * @param type $serviceId
     * @return type
     * @throws Exception
     */
    public function getBaIdFromCentreonService($serviceId)
    {
        $query = "SELECT description FROM services s WHERE s.service_id = $serviceId";
        $res = $this->_dbNdo->query($query);
        if ($res->numRows()) {
            while ($row = $res->fetchRow()) {
                $serviceDescription = $row['description'];
            }
        } else {
            throw new Exception('BA Not Found');
        }
        
        $serviceDescriptionExploded = explode('_', $serviceDescription);
        
        
        return $serviceDescriptionExploded[1];
    }
    
    /**
     * 
     * @param int $serviceId
     * @return int
     * @throws Exception
     */
    public function getBaIdFromCentreonConfigurationService($serviceId)
    {
        $query = "SELECT s.service_description FROM service s WHERE s.service_id = $serviceId";
        $res = $this->_db->query($query);
        if ($res->numRows()) {
            while ($row = $res->fetchRow()) {
                $serviceDescription = $row['service_description'];
            }
        } else {
            throw new Exception('BA Not Found');
        }
        
        $serviceDescriptionExploded = explode('_', $serviceDescription);
        
        
        return $serviceDescriptionExploded[1];
    }
    

     /**
     *
     * @return array
     */
    public function getVirtualHostId()
    {
        $virtualHostId = 0;
        $pollerId = 0;

        $queryPoller = "SELECT id FROM nagios_server ORDER BY localhost DESC LIMIT 1";
        $resPoller = $this->_db->query($queryPoller);
        if ($resPoller->numRows() > 0) {
            $rowPoller = $resPoller->fetchRow();
            $pollerId = $rowPoller['id'];
        }

        $query = "SELECT host_id FROM host WHERE host_name like '_Module_BAM_". $pollerId ."%' AND host_activate = '1' LIMIT 1";
        $res = $this->_db->query($query);
        if ($res->numRows()) {
            $row = $res->fetchRow();
            $virtualHostId = $row['host_id'];
        }

        return $virtualHostId;
    }

    public function getStatusList()
    {
        
    }
}
