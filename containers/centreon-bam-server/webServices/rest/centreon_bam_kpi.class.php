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
require_once $centreon_path . "/www/class/centreonHost.class.php";
require_once $centreon_path . "/www/class/centreonMeta.class.php";
require_once $centreon_path . "/www/class/centreonService.class.php";

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
//require_once $centreon_path . "www/modules/centreon-bam-server/core/common/header.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/common/functions.php";
require_once $centreon_path."www/class/centreonDuration.class.php";


class CentreonBamKpi extends CentreonWebService
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
    protected $kpiObj;

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
    protected $hostObj;
    
    /**
     *
     * @var type 
     */
    protected $serviceObj;

    /**
     *
     * @var type 
     */
    protected $tabKpiType;

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
        $this->kpiObj = new CentreonBam_Kpi($this->pearDB);
        $this->hostObj = new CentreonBam_Host($this->pearDB);
        $this->serviceObj = new CentreonBam_Service($this->pearDB);
        $this->metaObj = new CentreonBam_Meta($this->pearDB);
        $this->boolObj = new CentreonBam_Boolean($this->pearDB);

        $this->iconObj = new CentreonBam_Icon($this->pearDB);

        $this->options = $this->optionsObj->getUserOptions();
        $this->userBaList = $this->options['overview'];
        
        
        $this->tabKpiType = array(
            0 => _("Regular Service"),
            1 => _("Meta Service"),
            2 => _("Business Activity"),
            3 => _('Boolean KPI')
        );
        $this->tabBaStatus = array(
            0 => "OK",
            1 => "Warning",
            2 => "Critical",
            3 => "Unknown"
        );
        $this->tabBaService = array(
            "OK" => "#88b917",
            "Warning" => "#ff9a13",
            "Critical" => "#e00b3d",
            "Unknown" => "#bcbdc0"
        );

        $this->durationObj = new CentreonDuration();
    }



    /**
     *
     * @return array
     */
    public function getElementListByType()
    {
        $elementList = array();

        if (false === isset($this->arguments['type'])) {
            throw new Exception('Missing parameter : kpi type');
        } else {
            $type = $this->arguments['type'];
        }

        switch ($type) {
            case '0':
                $hostObj = new CentreonHost($this->pearDB);
                $elementList = $hostObj->getList();
                break;
            case '1':
                $metaObj = new CentreonMeta($this->pearDB);
                $elementList = $metaObj->getList();
                break;
            case '2':
                $baObj = new CentreonBam_Ba($this->pearDB, null, $this->pearDBMonitoring);
                $elementList = $baObj->getBA_list();
                break;
            case '3':
                $boolObj = new CentreonBam_Boolean($this->pearDB);
                $elementList = $boolObj->getList();
                break;
            default:
                throw new Exception('Unknown kpi type : ' . $type);
                break;
        }

        return $elementList;
    }

    /**
     * 
     * @return array
     */
    public function getStatusList()
    {
        $kpiStatusList = $this->loadStatusFromDb();
        return $kpiStatusList;
    }
    
    /**
     * 
     * @return string
     */
    private function loadStatusFromDb()
    {
        $kpiStatusList = array();
        
        if (false === isset($this->arguments['sid'])) {
            throw new Exception('Invalid Session');
        } else {
            $sessionId = $this->arguments['sid'];
        }
        
        if (false === isset($this->arguments['ba_id'])) {
            throw new Exception('No business Activity selected');
        } else {
            $baId = $this->arguments['ba_id'];
        }


        /* Let's get the kpi first */
        $query = "SELECT id_ba, kpi_id, activate, kpi_type, config_type, drop_warning, drop_critical, drop_unknown, " .
            "drop_warning_impact_id, in_downtime, drop_critical_impact_id, drop_unknown_impact_id, " .
            " host_id, service_id, id_indicator_ba, meta_id, boolean_id, current_status, last_impact " .
            "FROM `mod_bam_kpi` kpi " . 
            "WHERE id_ba = " . $this->pearDB->escape($baId) . " " . 
            $this->baAclObj->queryBuilder("AND", "id_ba", $this->baAclObj->getBaStr()) . 
            " AND activate = '1' " . 
            "ORDER BY kpi_type ";

        $DBRES = $this->pearDB->query($query);

        $kpiTab = array();
        $svcStr = "";
        $aSvc = array();
        
        while ($row = $DBRES->fetchRow()) {
            if ($row['config_type'] == '1') {
                $kpiTab[$row['kpi_id']]['warning_impact'] = $row['drop_warning'];
                $kpiTab[$row['kpi_id']]['critical_impact'] = $row['drop_critical'];
                $kpiTab[$row['kpi_id']]['unknown_impact'] = $row['drop_unknown'];
            } else {
                $kpiTab[$row['kpi_id']]['warning_impact'] = $this->kpiObj->getCriticityImpact($row['drop_warning_impact_id']);
                $kpiTab[$row['kpi_id']]['critical_impact'] = $this->kpiObj->getCriticityImpact($row['drop_critical_impact_id']);
                $kpiTab[$row['kpi_id']]['unknown_impact'] = $this->kpiObj->getCriticityImpact($row['drop_unknown_impact_id']);
            }
            $kpiTab[$row['kpi_id']]['type'] = $row['kpi_type'];

            switch ($row['kpi_type']) {
                case "0" :
                    $hostId = $row['host_id'];
                    $svcId = $row['service_id'];
                    $kpiId = $row['kpi_id'];
                    $kpiTab[$row['kpi_id']]['url'] = "#";
                    break;
                case "1" :
                    $hostId = $this->metaObj->getCentreonHostMetaId($this->pearDBMonitoring);
                    $svcId = $this->metaObj->getCentreonServiceMetaId($row['meta_id'], $this->pearDBMonitoring);
                    $kpiId = $row['kpi_id'];
                    $kpiTab[$row['kpi_id']]['url'] = "#";
                    break;
                case "2" :
                    $hostId = $this->baObj->getCentreonHostBaId($row['id_indicator_ba']);
                    $svcId = $this->baObj->getCentreonServiceBaId($row['id_indicator_ba'], $hostId);
                    $kpiId = $row['kpi_id'];
                    $kpiTab[$row['kpi_id']]['url'] = "javascript: showBADetails('" . $row['id_indicator_ba'] . "', 0);";
                    $kpiTab[$row['kpi_id']]['originalName'] = $this->baObj->getBA_Name($row['id_indicator_ba']);
                    break;
                case "3" :
                    $hName = "";            
                    $kpiId = $row['kpi_id'];
                    $kpiTab[$kpiId]['booleanData'] = $this->boolObj->getData($row['boolean_id']);
                    $kpiTab[$kpiId]['sName'] = $kpiTab[$kpiId]['booleanData']['name'];
                    $kpiTab[$kpiId]['url'] = "#";
                    break;
            }
            $kpiTab[$kpiId]['current_state'] = $row['current_status'];
            $kpiTab[$kpiId]['last_impact'] = $row['last_impact'];


            $kpiTab[$kpiId]["downtime"] = $row['in_downtime'];


            if (isset($hostId)) {
                $kpiTab[$kpiId]['hostId'] = $hostId;
            }
            if (isset($svcId) && $svcId > 0) {
                $kpiTab[$kpiId]['svcId'] = $svcId;
                $aSvc[] = $svcId;
            }
        }
        
        if (count($aSvc) > 0) {
            $svcStr = implode(",", $aSvc);

            $query2 = "SELECT h.name as hName, h.host_id as host_id, s.description as sName, " .
                "s.service_id as service_id, s.output, s.last_hard_state as current_state," .
                "h.state as hstate, h.state_type as hstate_type, s.last_check, s.next_check," .
                "s.perfdata, s.last_check, s.next_check, s.last_state_change, " .
                "s.acknowledged as problem_has_been_acknowledged, s.scheduled_downtime_depth, s.icon_image " .
                "FROM services s, hosts h " .
                "WHERE s.host_id = h.host_id " .
                "AND s.service_id IN ($svcStr) " .
                "AND s.enabled = 1 AND h.enabled = 1 " .
                "ORDER BY hName, sName ";
            $DBRES2 = $this->pearDBMonitoring->query($query2);
        }

        $baName = $this->baObj->getBA_Name($baId);

        $query3 = "SELECT id_ba_group FROM mod_bam_bagroup_ba_relation " .
            "WHERE id_ba = '".$baId."' ".$this->baAclObj->queryBuilder("AND", "id_ba_group", $this->baAclObj->getBaGroupStr());
        $DBRES3 = $this->pearDB->query($query3);
        $baGId = 0;
        if ($DBRES3->numRows()) {
            $row = $DBRES3->fetchRow();
            $baGId = $row['id_ba_group'];
        }
        
        $currentStatus = array();
        
        $currentStatus["host_id"] = $this->baObj->getCentreonHostBaId($baId);
        $currentStatus["service_id"] = $this->baObj->getCentreonServiceBaId($baId, $currentStatus["host_id"]);
        $currentStatus["kpi_header_label"] = _("Key Performance Indicators");
        $currentStatus["indicator_label"] = _("Indicator");
        $currentStatus["ba_label"] = _("Business Activity");
        $currentStatus["ba_name"] = $baName;
        $currentStatus["information_label"] = _("State Information");
        $currentStatus["indicator_type_label"] = _("Type");
        $currentStatus["output_label"] = _("Output");
        $currentStatus["status_label"] = _("Status");
        $currentStatus["impact_label"] = _("Impact");
        $currentStatus["warning_threshold_label"] = _("Warning Threshold");
        $currentStatus["critical_threshold_label"] = _("Critical Threshold");
        $currentStatus["node_icon"] = "./modules/centreon-bam-server/core/common/images/node.gif";
        $currentStatus["tp_icon"] = "./modules/centreon-bam-server/core/common/images/calendar.gif";
        $currentStatus["heart_icon"] = "./modules/centreon-bam-server/core/common/images/health.png";
        $currentStatus["reporting_period_label"] = _("Reporting Period");
        $currentStatus["ba_health_label"] = _("Health");
        $currentStatus["link_label"] = _("Links");
        $currentStatus["performances_label"] = _("Performance");
        $currentStatus["graph_label"] = _("Graph");
        $currentStatus["reporting_label"] = _("Reporting");
        $currentStatus["reporting_link"] = "./main.php?p=20702&period=yesterday&item=" . $baId;
        $currentStatus["reporting_link_allowed"] = $this->baAclObj->page("20702");
        $currentStatus["log_link"] = "./main.php?p=20703&item=" . $baId;
        $currentStatus["log_link_allowed"] = $this->baAclObj->page("20703");
        $currentStatus["graph_link"] = "./main.php?p=20401&mode=0&ba_id=" . $baId . "&ba_g_id=" . $baGId;
        $currentStatus["graph_link_allowed"] = $this->baAclObj->page("20401");
        $currentStatus["logs_label"] = _("Logs");
        $currentStatus["main_ba_id"] = $baId;
        $currentStatus["ba_name"] = $this->baObj->getBA_Name($baId);
        $currentStatus["svc_index"] = getMyIndexGraph4Service(
            getCentreonBaHostName($this->pearDB),
            "ba_" . $baId,
            $this->pearDBMonitoring
        );
        
        
        /*
        *  Get thresholds and periods
        */
        $query3 = "SELECT * FROM mod_bam WHERE ba_id = " . $this->pearDB->escape($baId) . " LIMIT 1";
        $DBRES3 = $this->pearDB->query($query3);
        if ($DBRES3->numRows()) {
            $row3 = $DBRES3->fetchRow();
            $currentStatus["warning_threshold"] = $row3['level_w'];
            $currentStatus["critical_threshold"] = $row3['level_c'];
            $currentStatus["reporting_period"] = $this->tpObj->getTimePeriodName($row3['id_reporting_period']) .
                " [".$this->tpObj->getTodayTimePeriod($row3["id_reporting_period"]) .
                "]";
            $currentStatus["health"] = $row3['current_level'];
            $health_icon = "";
            $pending = '0';
            switch ($row3['current_status']) {
                case "0" : $health_icon .= "#88b917";
                    break;
                case "1" : $health_icon .= "#ff9a13";
                    break;
                case "2" : $health_icon .= "#e00b3d";
                    break;
                default : $health_icon .= "#2ad1d4";
                    $pending = '1';
                    break;
            }
            $currentStatus["pending"] = $pending;
            $currentStatus["health_icon"] = $health_icon;
        }

        /*
         *  Get health
         */
        $hostId = $this->baObj->getCentreonHostBaId($baId);
        $iService =  $this->baObj->getCentreonServiceBaId($baId, $hostId);
        $query4 = "SELECT s.perfdata, s.icon_image, s.state as current_state, s.scheduled_downtime_depth " .
            "FROM services s " .
            "WHERE s.service_id = " . $iService . " LIMIT 1";

        $DBRES4 = $this->pearDBMonitoring->query($query4);
        if ($DBRES4->numRows()) {
            $row4 = $DBRES4->fetchRow();
            $currentStatus["ba_icon"] = $row4['icon_image'];
        } else {
            $currentStatus["ba_icon"] =  "";
        }

       $style = "list_two";
       $orderTab = array();
       $orderImpact = array();

       foreach ($kpiTab as $key => $value) {
            $key = trim($key);
            if ($kpiTab[$key]['type'] == 3) {
                $orderTab[$key]["id"] = $key;
                $orderTab[$key]["ba_id"] = $key;
                $orderTab[$key]["status"] = $this->tabBaStatus[0];
                if ($kpiTab[$key]['current_state']) {
                    $exprReturnValue = $kpiTab[$key]['booleanData']['bool_state'];
                } else {
                    $exprReturnValue = $kpiTab[$key]['booleanData']['bool_state'] ? 0 : 1;
                }
                $orderTab[$key]["output"] = sprintf(_('This KPI returns %s'), $exprReturnValue ? _('True') : _('False'));
                $orderTab[$key]["type_string"] = $this->tabKpiType[$kpiTab[$key]['type']];
                $orderTab[$key]["type"] = $kpiTab[$key]['type'];
                $orderTab[$key]["icon"] = "./modules/centreon-bam-server/core/common/images/dot-chart.gif";
                $orderTab[$key]["url"] = $kpiTab[$key]['url'];
                $orderTab[$key]["hname"] = "";
                $orderTab[$key]["sname"] = $kpiTab[$key]['sName'];
                $orderTab[$key]["ack"] = "";
                $orderTab[$key]["downtime"] = $kpiTab[$key]['downtime'];
                $orderTab[$key]["warning_impact"] = "";
                $orderTab[$key]["critical_impact"] = $kpiTab[$key]['critical_impact'];
                $orderTab[$key]["unknown_impact"] = "";
                $orderTab[$key]["last_impact"] = $kpiTab[$key]['last_impact'];
                if ($kpiTab[$key]['current_state']) {
                    $orderTab[$key]["status"] = $this->tabBaStatus[2];
                }
                $orderTab[$key]["status_color"] = $this->tabBaService[$orderTab[$key]["status"]];
                $orderTab[$key]["ba_action"] = "";
                $impact = $kpiTab[$key]['last_impact'];
                $orderTab[$key]["impact"] = $impact;
                if (isset($orderTab[$key]["last_impact"])) {
                    $orderImpact[$key] = $orderTab[$key]["last_impact"];
                } else {
                    $orderImpact[$key] = "";
                }
               unset($kpiTab[$key]);
           }
       }

        if (count($aSvc) > 0) {
            while ($row2 = $DBRES2->fetchRow()) {
                foreach ($kpiTab as $key => $value) {
                   $key = trim($key);
                    if (!$key) {
                        continue;
                    }
                    if (isset($kpiTab[$key]) && $kpiTab[$key]['hostId'] == $row2['host_id'] && $kpiTab[$key]['svcId'] == $row2['service_id']) {
                        $orderTab[$key]["id"] = $key;
                        $orderTab[$key]["ba_id"] = $key;
                        if (isset($this->tabBaStatus[$kpiTab[$key]['current_state']])) {
                            $orderTab[$key]["status"] = $this->tabBaStatus[$kpiTab[$key]['current_state']];
                        } else {
                            $orderTab[$key]["status"] = "";
                        }

                        $orderTab[$key]['output'] = $row2['output'];
                        $orderTab[$key]["type_string"] = $this->tabKpiType[$kpiTab[$key]['type']];
                        $orderTab[$key]["type"] = $kpiTab[$key]['type'];
                        if ($row2['icon_image'] != '') {
                            $orderTab[$key]["icon"] = './img/media/' . $row2['icon_image'];
                        } else {
                            $orderTab[$key]["icon"] = './img/icons/service.png';
                        }
                        $orderTab[$key]["url"] = $kpiTab[$key]['url'];
                        $orderTab[$key]["hname"] = $row2['hName'];
                        $orderTab[$key]["sname"] = $row2['sName'];
                        $orderTab[$key]["ack"] = $row2['problem_has_been_acknowledged'];

                        $orderTab[$key]["downtime"] = $row2['scheduled_downtime_depth'];

                        $orderTab[$key]["warning_impact"] = $kpiTab[$key]['warning_impact'];
                        $orderTab[$key]["critical_impact"] = $kpiTab[$key]['critical_impact'];
                        $orderTab[$key]["unknown_impact"] = $kpiTab[$key]['unknown_impact'];
                        if (in_array($orderTab[$key]["status"], $this->tabBaStatus)) {
                            $orderTab[$key]["status_color"] = $this->tabBaService[$orderTab[$key]["status"]];
                        } else {
                            $orderTab[$key]["status_color"] = "";
                        }
                        $impact = $kpiTab[$key]['last_impact'];
                        $orderTab[$key]["impact"] = $impact;
                        $link = "";
                        if ($orderTab[$key]["type"] == "2") {
                            $link = "javascript:updateGraph('" .
                                $this->baObj->getBA_id($kpiTab[$key]['originalName']) .
                                "')";
                            $orderTab[$key]["sname"] = $kpiTab[$key]['originalName'];
                        }
                        $orderTab[$key]["ba_action"] = $link;
                        $orderImpact[$key] = $impact;
                        unset($kpiTab[$key]);
                    }
                }
            }
        }

       arsort($orderImpact);
       $style = "list_two";
       $simpleKpiStatus = array();
       $currentStatus['kpi'] = array();
       $currentStatus['kpi_critical'] = array();
       foreach ($orderImpact as $key => $value) {
            foreach ($orderTab[$key] as $key2 => $value2) {
                $simpleKpiStatus[$key2] = $value2;
            }

            if ($orderTab[$key]['downtime'] != 0) {
                $style = "line_downtime";
            } else if ($orderTab[$key]['ack'] != 0) {
                $style = "line_ack";
            } else {
                $style == "list_one" ? $style = "list_two" : $style = "list_one";
            }

            $simpleKpiStatus["tr_style"] = $style;
            $currentStatus['kpi'][] = $simpleKpiStatus;

            if ($simpleKpiStatus['impact'] > 0) {
                $currentStatus['kpi_critical'][] = $simpleKpiStatus;
            }
        }
        $kpiStatusList = $currentStatus;
        
        return $kpiStatusList;
    }
}
