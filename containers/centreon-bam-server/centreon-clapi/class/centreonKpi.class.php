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

namespace CentreonBam\CentreonClapi;

require_once "centreonObject.class.php";
require_once "Centreon/Object/Host/Host.php";
require_once "Centreon/Object/Service/Service.php";
require_once "Centreon/Object/Relation/Host/Service.php";
require_once "Centreon/Object/Meta/Service.php";
require_once realpath(dirname(__FILE__)."/../../core/class/models/Kpi.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/Ba.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/BooleanRule.php");
require_once realpath(dirname(__FILE__)."/../../core/class/CentreonBam/Kpi.php");

/**
 *
 * @author kevin duret <kduret@centreon.com>
 *
 */
class CentreonKpi extends \CentreonClapi\CentreonObject {

    private $_DB;
    private $_DBC;
    private $centreon_path;
    const ORDER_TYPE = 0;
    const ORDER_OBJECT_ID = 1;
    const ORDER_ID_BA = 2;
    const ORDER_WARNING = 3;
    const ORDER_CRITICAL = 4;
    const ORDER_UNKNOWN = 5;
    const NB_UPDATE_PARAMS = 5;
    const CONFIG_TYPE = 0;
    const NOTSAMEPOLLER = "The service and BA are not linked in the same poller";
    
    public static $aDepends = array(
        'BOOLEANRULE',
        'BA',
        'HOST',
        'SERVICE'
    );

    /**
     * Constructor
     * @param CentreonDB $DB
     * @param string $centreon_path
     * @param CentreonDB $DBC
     * @return void
     */
    public function __construct($DB, $centreon_path)
    {    
        parent::__construct();
        $this->object = new \Kpi();
        $this->_DB = $DB;

        $this->insertParams = array(
            'kpi_type', 
            'object_id', 
            'id_ba', 
            'drop_warning', 
            'drop_critical', 
            'drop_unknown'
        );

        $this->params = array(
            'activate' => '1'
        );
        $this->action = "KPI";

        $this->hostObj = new \Centreon_Object_Host();
        $this->serviceObj = new \Centreon_Object_Service();
        $this->metaObj = new \Centreon_Object_Meta_Service();
        $this->baObj = new \Ba();
        $this->booleanObj = new \BooleanRule();

        $this->impacts = $this->object->getImpacts();
        $this->impactAssoc = array(
            'null' => '1',
            'weak' => '2',
            'minor' => '3',
            'major' => '4',
            'critical' => '5',
            'blocking' => '6'
        );

        $this->type = array(
            0 => 'service',
            1 => 'metaservice',
            2 => 'ba',
            3 => 'boolean',
            'service' => '0',
            'metaservice' => '1',
            'ba' => '2',
            'boolean' => '3'
        );

        $this->mode = array(
            0 => 'regular',
            1 => 'advanced',
            'regular' => '0',
            'advanced' => '1'
        );
        $this->nbOfCompulsoryParams = count($this->insertParams);
        $this->exportExcludedParams = array_merge($this->insertParams, array($this->object->getPrimaryKey(), 'host_id', 'service_id', 'meta_id', 'boolean_id', 'id_indicator_ba', 'id_ba'));
    }

    /**
     * Add a kpi
     *
     * @param string $parameters
     * @return void
     * @throws CentreonClapiException
     */
    public function add($parameters)
    {
        $params = explode($this->delim, $parameters);

        if (count($params) < ($this->nbOfCompulsoryParams - 2)) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }

        $addParams = array();
        
        $addParams['kpi_type'] = is_numeric($params[self::ORDER_TYPE]) ? $params[self::ORDER_TYPE] : $this->type[$params[self::ORDER_TYPE]];

        if (($addParams['kpi_type'] != 3) && (count($params) < $this->nbOfCompulsoryParams)) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }
        
        $impactedBa = $this->baObj->getIdByParameter('name', array($params[self::ORDER_ID_BA]));
        
        if (isset($impactedBa[0])) {
            $addParams['id_ba'] = $impactedBa[0];
        } else {
            throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND . ':' . $params[self::ORDER_ID_BA]);
        }

        $addParams['object_id'] = $params[self::ORDER_OBJECT_ID];
        if ($addParams['kpi_type'] == 0) {
            $object = explode('|', $addParams['object_id']);
            
            if (isset($object[0]) && isset($object[1])) {
                $hostName = $object[0];
                $serviceDesc = $object[1];
                $relObject = new \Centreon_Object_Relation_Host_Service();
                $elements = $relObject->getMergedParameters(array("host_id"), array("service_id"), -1, 0, null, null, array("host_name" => $hostName,
                    "service_description" => $serviceDesc), "AND");
                if (!count($elements)) {
                    throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND . ":" . $hostName . "/" . $serviceDesc);
                }
                $addParams['host_id'] = $elements[0]['host_id'];
                $addParams['service_id'] = $elements[0]['service_id'];
                    
                $isLinked = $this->object->IsLinked($addParams['id_ba'], $addParams['host_id'], $addParams['service_id']);
                if ($isLinked) {
                    throw new \CentreonClapi\CentreonClapiException(self::OBJECTALREADYLINKED);
                }

                $oKpi = new \CentreonBAM_Kpi($this->_DB);
                $bCheck = $oKpi->checkKpiTypeService(array($addParams['id_ba']), $addParams['host_id']);
                if (!$bCheck) {
                    throw new \CentreonClapi\CentreonClapiException(self::NOTSAMEPOLLER);
                }
            } else {
                throw new \CentreonClapi\CentreonClapiException("Can't parse object name");
            }
            
        } else if ($addParams['kpi_type'] == 1) {
            $metaservice = $this->metaObj->getIdByParameter('meta_name', array($addParams['object_id']));
            if (isset($metaservice[0])) {
                $addParams['meta_id'] = $metaservice[0];
                
                $isLinked = $this->object->IsMetaLinkedToBa($addParams['id_ba'], $addParams['meta_id']);
                if ($isLinked) {
                    throw new \CentreonClapi\CentreonClapiException(self::OBJECTALREADYLINKED);
                }
            } else {
                throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND);
            }
        } else if ($addParams['kpi_type'] == 2) {
            $ba = $this->baObj->getIdByParameter('name', array($addParams['object_id']));
            if (isset($ba[0])) {
                if($this->object->checkInfiniteLoop($ba[0], $addParams['id_ba'])){
                    $addParams['id_indicator_ba'] = $ba[0];
                    $oKpi = new \CentreonBAM_Kpi($this->_DB);

                    $isLinked = $this->object->isBaLinked($addParams['id_ba'], $addParams['id_indicator_ba']);
                    if ($isLinked) {
                        throw new \CentreonClapi\CentreonClapiException(self::OBJECTALREADYLINKED);
                    }

                    $bCheck = $oKpi->checkKpiTypeBa(array($addParams['id_ba']), $ba[0]);
                    if (!$bCheck) {
                        throw new \CentreonClapi\CentreonClapiException(self::NOTSAMEPOLLER);
                    }
                } else {
                    throw new \CentreonClapi\CentreonClapiException("circular dependency");
                }
            } else {
                throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND);
            }
        } else if ($addParams['kpi_type'] == 3) {
            $boolean = $this->booleanObj->getIdByParameter('name', array($addParams['object_id']));
            if (isset($boolean[0])) {
                $addParams['boolean_id'] = $boolean[0];
                
                $isLinked = $this->object->IsRuleLinkedToBa($addParams['id_ba'], $addParams['boolean_id']);

                if ($isLinked) {
                    throw new \CentreonClapi\CentreonClapiException(self::OBJECTALREADYLINKED);
                }
               
                $oKpi = new \CentreonBAM_Kpi($this->_DB);
                $bCheck = $oKpi->checkKpiTypeBoolean(array($addParams['id_ba']), $addParams['boolean_id']);

                if (!$bCheck) {
                    throw new \CentreonClapi\CentreonClapiException(self::NOTSAMEPOLLER);
                }          
            } else {
                throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND);
            }
        }

        unset($addParams['object_id']);

        $addParams['config_type'] = self::CONFIG_TYPE;

        if ($addParams['kpi_type'] == 3) {
            if (is_numeric($params[self::ORDER_WARNING]) || $params[self::ORDER_WARNING] == "") {
                $addParams['config_type'] = 1;
                $addParams['drop_critical'] = $params[self::ORDER_WARNING];
            } else {
                $addParams['drop_critical_impact_id'] = $this->impactAssoc[strtolower($params[self::ORDER_WARNING])];
            } 
        } else {
            if (is_numeric($params[self::ORDER_WARNING]) || $params[self::ORDER_WARNING] == "") {
                $addParams['config_type'] = 1;
                $addParams['drop_warning'] = $params[self::ORDER_WARNING];
            } else {
                $addParams['drop_warning_impact_id'] = $this->impactAssoc[strtolower($params[self::ORDER_WARNING])];
            }

            if (is_numeric($params[self::ORDER_CRITICAL]) || $params[self::ORDER_CRITICAL] == "") {
                $addParams['config_type'] = 1;
                $addParams['drop_critical'] = $params[self::ORDER_CRITICAL];
            } else {
                $addParams['drop_critical_impact_id'] = $this->impactAssoc[strtolower($params[self::ORDER_CRITICAL])];
            }

            if (is_numeric($params[self::ORDER_UNKNOWN]) || $params[self::ORDER_UNKNOWN] == "") {
                $addParams['config_type'] = 1;
                $addParams['drop_unknown'] = $params[self::ORDER_UNKNOWN];
            } else {
                $addParams['drop_unknown_impact_id'] = $this->impactAssoc[strtolower($params[self::ORDER_UNKNOWN])];
            }
        }

        $this->params = array_merge($this->params, $addParams);

        parent::add();
    }

    /**
     * Set parameters
     *
     * @param string $parameters
     * @return void
     * @throws CentreonClapiException
     */
    public function setparam($parameters = null)
    {
        $params = explode($this->delim, $parameters);
        if (count($params) < self::NB_UPDATE_PARAMS) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }
        $type = is_numeric($params[self::ORDER_TYPE]) ? $params[self::ORDER_TYPE] : $this->type[$params[self::ORDER_TYPE]];
        $kpiId = self::getKpiId($type, $params[1], $params[2]);
        
        switch ($params[3]) {
            case "type":
            case "kpi_type":
                throw new \CentreonClapi\CentreonClapiException("KPI type can't be changed");
                break;
            case "warning_impact":
                if (is_numeric($params[4]) || $params[4] == "") {
                    $params[3] = "drop_warning";
                } else {
                    $params[4] = $this->impactAssoc[strtolower($params[4])];
                    $params[3] = "drop_warning_impact_id";
                }
                break;
            case "critical_impact":
                if (is_numeric($params[4]) || $params[4] == "") {
                    $params[3] = "drop_critical";
                } else {
                    $params[4] = $this->impactAssoc[strtolower($params[4])];
                    $params[3] = "drop_critical_impact_id";
                }
                break;
            case "unknown_impact":
                if (is_numeric($params[4]) || $params[4] == "") {
                    $params[3] = "drop_unknown";
                } else {
                    $params[4] = $this->impactAssoc[strtolower($params[4])];
                    $params[3] = "drop_unknown_impact_id";
                }
                break;
            case "impacted_ba":
                $params[3] = "id_ba";
                $updateImpactedBa = $this->baObj->getIdByParameter('name', array($params[4]));
                if (isset($updateImpactedBa[0])) {
                    $params[4] = $updateImpactedBa[0];
                    $aBaSelected = array($params[4]);
                    $bCheck = true;
                    
                    $oKpi = new \CentreonBam_Kpi($this->_DB);
                    
                    if ($type == '0') {
                        $object = explode('|', $params[1]);
                        $hostName = $object[0];
                        $serviceDesc = $object[1];
                        
                        $relObject = new \Centreon_Object_Relation_Host_Service();
                        $elements = $relObject->getMergedParameters(array("host_id"), array("service_id"), -1, 0, null, null, array("host_name" => $hostName,
                            "service_description" => $serviceDesc), "AND");
                        if (!count($elements)) {
                            throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND . ":" . $hostName . "/" . $serviceDesc);
                        }
                        $bCheck = $oKpi->checkKpiTypeService($aBaSelected, $elements[0]['host_id']);
                        if (!$bCheck) {
                            throw new \CentreonClapi\CentreonClapiException(self::NOTSAMEPOLLER);
                        }
                    } else if ($type == '2') {
                        $ba = $this->baObj->getIdByParameter('name', array($params[1]));
                        
                        if (isset($ba[0])) { 
                            $bCheck = $oKpi->checkKpiTypeBa($aBaSelected, $ba[0]);
                            if (!$bCheck) {
                                throw new \CentreonClapi\CentreonClapiException(self::NOTSAMEPOLLER);
                            }
                        }
                    } else if ($type == '3') {
                        $boolean = $this->booleanObj->getIdByParameter('name', array($params[1]));
                        if (isset($boolean[0])) {
                            $bCheck = $oKpi->checkKpiTypeBoolean($aBaSelected, $boolean[0]);

                            if (!$bCheck) {
                                throw new \CentreonClapi\CentreonClapiException(self::NOTSAMEPOLLER);
                            }
                        }
                    }
                } else {
                    throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND . ":" . $params[4]);
                }
                break;
            case "enable":
                $params[3] = "activate";
                break;
        }

        $updateParams = array($params[3] => $params[4]);
        parent::setparam($kpiId, $updateParams);
    }

    /**
     * Set impact mode
     * regular or advanced
     *
     * @param string $parameters
     * @return void
     * @throws CentreonClapiException
     */
    public function setimpactmode($parameters = null)
    {
        $params = explode($this->delim, $parameters);
        if (count($params) < 4) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }

        $type = is_numeric($params[self::ORDER_TYPE]) ? $params[self::ORDER_TYPE] : $this->type[$params[self::ORDER_TYPE]];

        $kpiId = self::getKpiId($type, $params[1], $params[2]);

        $mode = is_numeric($params[3]) ? $params[3] : $this->mode[$params[3]];

        $updateParams = array('config_type' => $mode);
        parent::setparam($kpiId, $updateParams);
    }

    /**
     * Display all kpi
     *
     * @param string $parameters
     */
    public function show($parameters = null)
    {
        $params = array('kpi_id', 'kpi_type', 'kpi_name', 'impacted_ba', 'warning_impact', 'critical_impact', 'unknown_impact');
        $paramString = str_replace("kpi_", "", implode($this->delim, $params));
        echo $paramString . "\n";

        $indicators = array();
        $indicators = array_merge($indicators, $this->object->getServiceIndicators());
        $indicators = array_merge($indicators, $this->object->getMetaserviceIndicators());
        $indicators = array_merge($indicators, $this->object->getBaIndicators());
        $indicators = array_merge($indicators, $this->object->getBooleanIndicators());

        foreach ($indicators as $indicator) {
            $indicator['kpi_type'] = $this->type[$indicator['kpi_type']];
            if (!is_null($indicator['drop_warning_impact_id'])) {
                $indicator['drop_warning'] = $this->impacts[$indicator['drop_warning_impact_id']];
            }
            unset($indicator['drop_warning_impact_id']);
            if (!is_null($indicator['drop_critical_impact_id'])) {
                $indicator['drop_critical'] = $this->impacts[$indicator['drop_critical_impact_id']];
            }
            unset($indicator['drop_critical_impact_id']);
            if (!is_null($indicator['drop_unknown_impact_id'])) {
                $indicator['drop_unknown'] = $this->impacts[$indicator['drop_unknown_impact_id']];
            }
            unset($indicator['drop_unknown_impact_id']);

            echo implode($this->delim, $indicator) . "\n";
        }
    }

    /**
     * Export
     *
     * @return void
     */
    public function export()
    {
        $elements = $this->object->getList("*", -1, 0);
        foreach ($elements as $element) {
            $addStr = $this->action . $this->delim . "ADD";
            if ($element['kpi_type'] == 0) {
                $host = $this->hostObj->getParameters($element['host_id'], 'host_name');
                $service = $this->serviceObj->getParameters($element['service_id'], 'service_description');
                $element['object_id'] = $host['host_name'] . '|' . $service['service_description'];
            } else if ($element['kpi_type'] == 1) {
                $metaservice = $this->metaObj->getParameters($element['meta_id'], 'meta_name');
                $element['object_id'] = $metaservice['meta_name'];
            } else if ($element['kpi_type'] == 2) {
                $ba = $this->baObj->getParameters($element['id_indicator_ba'], 'name');
                $element['object_id'] = $ba['name'];
            } else if ($element['kpi_type'] == 3) {
                $boolean = $this->booleanObj->getParameters($element['boolean_id'], 'name');
                $element['object_id'] = $boolean['name'];
            } else {
                next;
            }

            $impactedBa = $this->baObj->getParameters($element['id_ba'], 'name');
            $element['id_ba'] = $impactedBa['name'];

            foreach ($this->insertParams as $param) {
                $addStr .= $this->delim . $element[$param];
            }

            $addStr .= "\n";
            echo $addStr;

            foreach ($element as $parameter => $value) {
                if (!in_array($parameter, $this->exportExcludedParams)) {
                    if (!is_null($value) && $value != "") {
                        $value = \CentreonUtils::convertLineBreak($value);
                        echo $this->action.$this->delim."setparam".$this->delim.$element['kpi_type'].$this->delim.$element['object_id'].$this->delim.$element['id_ba'].$this->delim.$parameter.$this->delim.$value."\n";
                    }
                }
            }
        }
    }

    /**
     * Del Action
     *
     * @param string $object
     * @return void
     * @throws CentreonClapiException
     */
    public function del($object)
    {
        $params = explode($this->delim, $object);
        if (count($params) < 3) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }

        $type = is_numeric($params[self::ORDER_TYPE]) ? $params[self::ORDER_TYPE] : $this->type[$params[self::ORDER_TYPE]];

        $kpiId = self::getKpiId($type, $params[1], $params[2]);

        $this->object->delete($kpiId);
    }

    /**
     * get kpi id
     *
     * @param string $type
     * @param string $object
     * @param string $impactedBa
     * @return void
     * @throws CentreonClapiException
     */
    public function getKpiId($type, $object, $impactedBa)
    {
        $objectParams = array();
        if ($type == 0) {
            $hostService = explode('|', $object);
            if (isset($hostService[0]) && isset($hostService[1])) {
                $hostName = $hostService[0];
                $serviceDesc = $hostService[1];
                $relObject = new \Centreon_Object_Relation_Host_Service();
                $elements = $relObject->getMergedParameters(array("host_id"), array("service_id"), -1, 0, null, null, array("host_name" => $hostName,
                    "service_description" => $serviceDesc), "AND");
                if (!count($elements)) {
                    throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND . ":" . $hostName . "/" . $serviceDesc);
                }

                $objectParams['host_id'] = $elements[0]['host_id'];
                $objectParams['service_id'] = $elements[0]['service_id'];
            } else {
                throw new \CentreonClapi\CentreonClapiException("Can't parse object name");
            }
        } else if ($type == 1) {
            $metaservice = $this->metaObj->getIdByParameter('meta_name', array($object));
            if (isset($metaservice[0])) {
                $objectParams['meta_id'] = $metaservice[0];
            } else {
                throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND);
            }
        } else if ($type == 2) {
            $ba = $this->baObj->getIdByParameter('name', array($object));
            if (isset($ba[0])) {
                $objectParams['id_indicator_ba'] = $ba[0];
            } else {
                throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND);
            }
        } else if ($type == 3) {
            $boolean = $this->booleanObj->getIdByParameter('name', array($object));
            if (isset($boolean[0])) {
                $objectParams['boolean_id'] = $boolean[0];
            } else {
                throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND);
            }
        } else {
            throw new \CentreonClapi\CentreonClapiException('Unrecognized type');
        }

        $impactedBaId = $this->baObj->getIdByParameter('name', array($impactedBa));
        if (isset($impactedBaId[0])) {
            $objectParams['id_ba'] = $impactedBaId[0];
        } else {
            throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND);
        }

        $kpi = $this->object->getList($parameterNames = "kpi_id", $count = -1, $offset = 0, $order = null, $sort = "ASC", $filters = $objectParams, $filterType = "AND");
        if (isset($kpi[0]['kpi_id'])) {
            $kpiId = $kpi[0]['kpi_id'];
        } else {
            throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND);
        }

        return $kpiId;
    }
}
