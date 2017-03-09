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

global $centreon_path;
require_once _CENTREON_PATH_ . "/www/class/centreonDB.class.php";
require_once $centreon_path . "/www/class/centreonBroker.class.php";

if (file_exists($centreon_path . "/www/include/common/webServices/rest/webService.class.php")) {
    // Centreon < 2.8
    require_once $centreon_path . "/www/include/common/webServices/rest/webService.class.php";
} else {
    // Centreon >= 2.8
    require_once $centreon_path . "/www/api/class/webService.class.php";
}

require_once $centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/Acl.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Utils.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/Loader.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/common/functions.php";
require_once $centreon_path."www/class/centreonDuration.class.php";


class CentreonBamBa  extends CentreonWebService
{
    /**
     *
     * @var type 
     */
    protected $pearDB;
    
    /**
     *
     * @var type 
     */
    protected $pearDBMonitoring;
    
    /**
     *
     * @var type 
     */
    protected $baObj;
    
    /**
     *
     * @var type 
     */
    protected $baAclObj;
    
    /**
     *
     * @var type 
     */
    protected $bvObj;
    
    /**
     *
     * @var type 
     */
    protected $durationObj;
    
    /**
     *
     * @var type 
     */
    protected $iconObj;
    
    /**
     *
     * @var type 
     */
    protected $tpObj;
    
    /**
     *
     * @var type 
     */
    protected $options;
    
    /**
     *
     * @var type 
     */
    protected $optionsObj;
    
    /**
     *
     * @var type 
     */
    protected $userBaList;
    
    /**
     *
     * @var type 
     */
    protected $tabBaStatus;
    
    /**
     *
     * @var type 
     */
    protected $tabBaService;
    
    /**
     *
     * @var type 
     */
    protected $centreon;

    /**
     * 
     * @global type $centreon
     */
    public function __construct()
    {
        global $centreon;
        $this->centreon = $centreon;
        $this->pearDBMonitoring = new CentreonDB('centstorage');

        parent::__construct();
        
        $cLoad = new CentreonBam_Loader(_CENTREON_PATH_ . 'www/modules/centreon-bam-server/core/class/Centreon/');
		$cLoadBam = new CentreonBam_Loader(_CENTREON_PATH_ . 'www/modules/centreon-bam-server/core/class/CentreonBam/');
        
        $this->baAclObj = new CentreonBam_Acl($this->pearDB, $this->centreon->user->user_id);
        $this->tpObj = new CentreonBam_TimePeriod($this->pearDB);
        $this->bvObj = new CentreonBam_BaGroup($this->pearDB);
        $this->optionsObj = new CentreonBam_Options($this->pearDB, $this->centreon->user->user_id);
        $this->baObj = new CentreonBam_Ba($this->pearDB, null, $this->pearDBMonitoring);

        $this->iconObj = new CentreonBam_Icon($this->pearDB);

        $this->options = $this->optionsObj->getUserOptions();
        $this->userBaList = $this->options['overview'];

        $this->tabBaStatus = array(
            0 => "OK", 
            1 => "Warning", 
            2 => "Critical", 
            3 => "Unknown",
            4 => "Pending"
        );

        $this->tabBaService = array(
            "OK" => 'service_ok',
            "Warning" => 'service_warning',
            "Critical" => 'service_critical',
            "Unknown" => 'service_unknown',
            "Pending" => 'pending'
        );
        
        $this->durationObj = new CentreonDuration();
    }

    /**
     * Get default report values
     * 
     * @param array $args
     */
    public function getList()
    {
        if (false === isset($this->arguments['q'])) {
            $q = '';
        } else {
            $q = $this->arguments['q'];
        }
        
        $query = "SELECT SQL_CALC_FOUND_ROWS name, ba_id "
            . "FROM `mod_bam` "
            . "WHERE name LIKE '%" . $this->pearDB->escape($q) . "%' ".
            $this->baAclObj->queryBuilder("AND", "ba_id", $this->baAclObj->getBaStr());

        $DBRESULT = $this->pearDB->query($query);
        
        $total = $this->pearDB->numberRows();
        
        $bas = array();
        while ($row = $DBRESULT->fetchRow()) {
            $bas[] = array(
                'id' => $row['ba_id'],
                'text' => $row['name']
            );
        }
        return array(
            'items' => $bas,
            'total' => $total
        );
    }
    
    /**
     * 
     * @return array
     */
    public function getStatusList()
    {
        $baStatusList = $this->loadStatusFromDb();
        return $baStatusList;
    }
    
    /**
     * 
     * @return string
     */
    private function loadStatusFromDb()
    {
        if (false === isset($this->arguments['bv_filter'])) {
            $bvFilter = 0;
        } else {
            $bvFilter = $this->arguments['bv_filter'];
        }
        
        if (false === isset($this->arguments['num'])) {
            $num = 0;
        } else {
            $num = $this->arguments['num'];
            if ($num === '') {
                $num = 0;
            }
        }
        
        if (false === isset($this->arguments['limit'])) {
            $limit = 30;
        } else {
            $limit = $this->arguments['limit'];
            if ($limit === '') {
                $limit = 30;
            }
        }
        
        if ($bvFilter == 0) {
            $query = "SELECT SQL_CALC_FOUND_ROWS * FROM `mod_bam` ba "
                . "left join mod_bam_bagroup_ba_relation br on br.id_ba = ba.ba_id "
                . "WHERE ba.activate = '1' "
                . $this->baAclObj->queryBuilder("AND", "ba_id", $this->baAclObj->getBaStr())
                . " GROUP BY ba.ba_id "
                . "ORDER BY ba.current_level, ba.name "
                . "LIMIT ".($num * $limit).",".$limit;
        } else {
            $query = "SELECT SQL_CALC_FOUND_ROWS * FROM `mod_bam` ba "
                . "WHERE activate = '1' "
                . "AND ba_id IN (SELECT id_ba FROM mod_bam_bagroup_ba_relation WHERE id_ba_group = '".$bvFilter."') "
                . $this->baAclObj->queryBuilder("AND", "ba_id", $this->baAclObj->getBaStr())
                . " ORDER BY ba.current_level, ba.name LIMIT ".($num * $limit).",".$limit;
        }
        
        $DBRES = $this->pearDB->query($query);
        $numRows = $this->pearDB->numberRows();
        
        $baStatusList = array();
        
        while ($row = $DBRES->fetchRow()) {
            $currentBaStatus = array();
            $baServiceDescription = 'ba_' . $row['ba_id'];

            $acknowledged = '0';
            $downtime = '0';

            $baHostId = $this->baObj->getCentreonHostBaId($row['ba_id']);
            $sql = 'SELECT name FROM hosts WHERE host_id = ' . $baHostId;
            $resHost = $this->pearDBMonitoring->query($sql);
            while ($dataHost = $resHost->fetchRow()) {
                $baHostName = $dataHost['name'];
            }

            $query2 = "SELECT s.acknowledged, s.scheduled_downtime_depth " .
                "FROM services s, hosts h " .
                "WHERE s.description = '" . $baServiceDescription . "' " .
                "AND h.name = '" . $baHostName . "' " .
                "AND s.host_id = h.host_id " .
                "AND s.enabled = '1'";
            $DBRES2 = $this->pearDBMonitoring->query($query2);
            if ($DBRES2->numRows()) {
                $row2 = $DBRES2->fetchRow();
                $acknowledged = $row2['acknowledged'];
                $downtime = $row2['scheduled_downtime_depth'];
            }
            
            $currentBaStatus["ba_id"] = $row['ba_id'];
            if (isset($row['id_ba_group'])) {
                $id_ba_group = $row['id_ba_group'];
            } else {
                $id_ba_group = "";
            }

            $currentBaStatus["ba_g_id"] = $id_ba_group;
            $currentBaStatus["name"] = $row['name'];
            $currentBaStatus["description"] = $row['description'];
            $currentBaStatus["current"] = $row['current_level'];
            $currentBaStatus["warning"] = $row['level_w'];
            $currentBaStatus["critical"] = $row['level_c'];
            
            if (isset($this->tabBaStatus[$row['current_status']])) {
                $status = $this->tabBaStatus[$row['current_status']];
            } else {
                $status = "";
            }
            $currentBaStatus["status"] = $status;
            $currentBaStatus["ack"] = $acknowledged;
            $currentBaStatus["acknowledged"] = $acknowledged;
            $currentBaStatus["downtime"] = $downtime;
            $currentBaStatus["notif"] = $row["notifications_enabled"];
            $currentBaStatus["reportingperiod"] = $this->tpObj->getTimePeriodName($row["id_reporting_period"]);

            $icon = $this->iconObj->getFullIconPath($row['icon_id']);
            if (is_null($icon)) {
                $currentBaStatus["icon"] = "./modules/centreon-bam-server/core/common/img/kpi-service.png";
            } else {
                $currentBaStatus["icon"] = $icon;
            }

            if ($row['last_state_change']) {
                $currentBaStatus["duration"] = $this->durationObj->toString(time() - $row['last_state_change']);
            } else {
                $currentBaStatus["duration"] = "N/A";
            }

            $svcIndex = getMyIndexGraph4Service($baHostName, $baServiceDescription, $this->pearDBMonitoring);
            $currentBaStatus["svc_index"] = $svcIndex;
            $currentBaStatus["ba_url"] = "./main.php?p=207&o=d&ba_id=".$row['ba_id'];

            if (is_numeric($row["current_status"]) &&
                isset($this->tabBaService[$this->tabBaStatus[$row["current_status"]]])
            ) {
                $status_badge = $this->tabBaService[$this->tabBaStatus[$row["current_status"]]];
            } else {
                $status_badge = "pending";
            }

            $currentBaStatus["status_badge"] = $status_badge;

            $style = '';
            // Stytle
            if ($downtime != 0) {
                $style .= "line_downtime";
            } else if ($acknowledged != 0) {
                $style .= "line_ack";
            } else {
                $style == "list_two" ? $style .= "list_one" : $style .= "list_two";
            }

            $currentBaStatus["tr_style"] = $style;
            $baStatusList[] = $currentBaStatus;
        }
        
        return $baStatusList;
    }
}
