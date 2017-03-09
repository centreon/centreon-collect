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
require_once realpath(dirname(__FILE__)."/../../core/class/CentreonBam/Ba.php");
require_once realpath(dirname(__FILE__)."/../../core/class/CentreonBam/Kpi.php");
require_once realpath(dirname(__FILE__)."/../../core/class/CentreonBam/Options.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/Ba.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/BaGroup.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/Relation/Contactgroup/Ba.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/Relation/Bagroup/Ba.php");
require_once realpath(dirname(__FILE__)."/../../core/class/Centreon/Command.php");
require_once ("Centreon/Object/Graph/Template/Template.php");
require_once ("Centreon/Object/Contact/Group.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/Relation/Instance/Ba.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/Relation/Timeperiod/Ba.php");
require_once ("Centreon/Object/Instance/Instance.php");
require_once "centreonInstance.class.php";

/**
 *
 * @author Julien Mathis
 *
 */
class CentreonBa extends \CentreonClapi\CentreonObject {

    private $_DB;
    private $_DBC;
    private $centreon_path;

    const ORDER_UNIQUENAME = 0;
    const ORDER_DESCRIPTION = 1;
    const ORDER_WARNING = 2;
    const ORDER_CRITICAL = 3;
    const ORDER_NOTIFICATION = 4;
    const NOTSAMEPOLLER = "The service and BA are not linked in the same poller";

    public static $aDepends = array(
        'INSTANCE'
    );

    public static $table = array(
        "icon_id" => "icon_image",
        "id_reporting_period" => "reporting_period",
        "id_notification_period" => "notification_period"
    );

    /**
     * Constructor
     * @param CentreonDB $DB
     * @param string $centreon_path
     * @param CentreonDB $DBC
     * @return void
     */
    public function __construct($DB, $centreon_path) {
        parent::__construct();
        $this->object = new \Ba();
        $this->_DB = $DB;
        
        $options = new \CentreonBAM_Options($DB, $oreon->user->user_id);
	    $opt = $options->getGeneralOptions();

        $this->params = array(
            'activate'               => '1',
            'notifications_enabled'  => '1'
        );
       
        if (!is_null($opt['id_reporting_period'])) {
            $this->params['id_reporting_period'] = $opt['id_reporting_period'];
        }
        
        if (!is_null($opt['id_notif_period'])) {
            $this->params['id_notification_period'] = $opt['id_notif_period'];
        }
        $this->action = "BA";
        $this->insertParams = array('name', 'description', 'level_w', 'level_c', 'notification_interval');
        $this->exportExcludedParams = array_merge($this->insertParams, array($this->object->getPrimaryKey(), 'icon_id', 'graph_id', 'dependency_dep_id'));
        $this->nbOfCompulsoryParams = count($this->insertParams);
    }

    /**
     * Add a business activity
     *
     * @param string $parameters
     * @return void
     * @throws CentreonClapiException
     */
    public function add($parameters) {
        $params = explode($this->delim, $parameters);
        
        if (count($params) < $this->nbOfCompulsoryParams) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }

        $addParams = array();
        $addParams[$this->object->getUniqueLabelField()] = $params[self::ORDER_UNIQUENAME];
        $addParams['description'] = $params[self::ORDER_DESCRIPTION];
        $addParams['level_w'] = $params[self::ORDER_WARNING];
        $addParams['level_c'] = $params[self::ORDER_CRITICAL];

        // Threshold validation
        if (!is_numeric($addParams['level_w']) || !is_numeric($addParams['level_c'])) {
            throw new \CentreonClapi\CentreonClapiException('Theshold range is 0-100');
        }
        self::checkThresholds($addParams['level_w'], $addParams['level_c']);

        $addParams['notification_interval'] = $params[self::ORDER_NOTIFICATION];
        $this->params = array_merge($this->params, $addParams);

        $this->checkParameters();
        $baId = parent::add();
    }

    /**
     * Display all business activities
     *
     * @param string $parameters
     * @return void
     */
    public function show($parameters = null) {
        $filters = array();
        if (isset($parameters)) {
            $filters[$this->object->getUniqueLabelField()] = "%" . $parameters . "%";
        }
        $params = array('ba_id', 'name', 'description', 'level_w', 'level_c');
        $paramString = str_replace("ba_", "", implode($this->delim, $params));
        echo $paramString . "\n";
        $elements = $this->object->getList($params, -1, 0, null, null, $filters, "AND");
        foreach ($elements as $tab) {
            echo implode($this->delim, $tab) . "\n";
        }
    }

    /**
     * Set parameters
     *
     * @param string $parameters
     * @return void
     * @throws CentreonClapiException
     */
    public function setparam($parameters = null) {
        $params = explode($this->delim, $parameters);
        if (count($params) < self::NB_UPDATE_PARAMS) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }
        if (($objectId = $this->getObjectId($params[self::ORDER_UNIQUENAME])) != 0) {
            switch ($params[1]) {
                case 'level_w':
                    $warningThreshold = $params[2];
                    $criticalThreshold = $this->object->getParameters($objectId, array('level_c'));
                    $criticalThreshold = $criticalThreshold['level_c'];
                    if (!is_numeric($warningThreshold)) {
                        throw new \CentreonClapi\CentreonClapiException('Theshold range is 0-100');
                    } else if (is_numeric($criticalThreshold)) {
                        self::checkThresholds($warningThreshold, $criticalThreshold);
                    }
                    break;
                case 'level_c':
                    $criticalThreshold = $params[2];
                    $warningThreshold = $this->object->getParameters($objectId, array('level_w'));
                    $warningThreshold = $warningThreshold['level_w'];
                    if (!is_numeric($criticalThreshold)) {
                        throw new \CentreonClapi\CentreonClapiException('Theshold range is 0-100');
                    } else if (is_numeric($warningThreshold)) {
                        self::checkThresholds($warningThreshold, $criticalThreshold);
                    }
                    break;
                case 'enable':
                    $params[1] = 'activate';
                    break;
                case "reporting_period":
                    $params[1] = "id_reporting_period";
                    $tpObj = new \CentreonTimePeriod();
                    $sTpName = htmlentities($params[2], ENT_QUOTES, "UTF-8");
                    $params[2] = $tpObj->getTimeperiodId($sTpName);
                    break;
                case "notification_period":
                    $params[1] = "id_notification_period";
                    $tpObj = new \CentreonTimePeriod();
                    $sTpName = htmlentities($params[2], ENT_QUOTES, "UTF-8");
                    $params[2] = $tpObj->getTimeperiodId($sTpName);
                    break;
                case "icon":
                case "icon_image":
                    $params[1] = "icon_id";
                    $params[2] = \CentreonUtils::getImageId($params[2]);
                    break;
                case "graph_id":
                    $graphObj = new \Centreon_Object_Graph_Template();
                    $tmp = $graphObj->getIdByParameter($graphObj->getUniqueLabelField(), $params[2]);
                    if (!count($tmp)) {
                        throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND . ":" . $params[2]);
                    }
                    $params[1] = "graph_id";
                    $params[2] = $tmp[0];
                    break;
                case "poller":
                    $instanceObj = new \CentreonInstance();
                    $instanceId = $instanceObj->getInstanceId($params[2]);
                    
                    if (isset($instanceId)) {
                        $instanceRelationObject = new \Centreon_Object_Relation_Instance_Ba();
                        $instanceRelationObject->insert($instanceId, $objectId);
                    }
                    break;
                case "event_handler_enabled": 
                    break;
                case "event_handler_command":
                    $commandObj = new \CentreonBAM_Command($this->_DB);
                    $tmp = $commandObj->getCommandId($params[2]);
                    if (!isset($tmp)) {
                        throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND . ":" . $params[2]);
                    } 
                    $params[2] = $tmp;
                    break;
                case "event_handler_agrs":
                    break;  
            }
            $updateParams = array($params[1] => $params[2]);
            parent::setparam($objectId, $updateParams);
        } else {
            throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND . ":" . $params[self::ORDER_UNIQUENAME]);
        }
    }

    /**
     * Export
     *
     * @return void
     */
    public function export() {
        $elements = $this->object->getList();
        $tpObj = new \Centreon_Object_Timeperiod();
        foreach ($elements as $element) {
            $addStr = $this->action . $this->delim . "ADD";
            foreach ($this->insertParams as $param) {
                $addStr .= $this->delim . $element[$param];
            }
            $addStr .= "\n";
            echo $addStr;
            foreach ($element as $parameter => $value) {
                if (!in_array($parameter, $this->exportExcludedParams) && !is_null($value) && $value != "") {

                    if ($parameter == "id_reporting_period" || $parameter == "id_notification_period") {
                        $tmpObj = $tpObj;
                    }
                    if (isset($tmpObj)) {
                        $tmp = $tmpObj->getParameters($value, $tmpObj->getUniqueLabelField());
                        if (isset($tmp) && isset($tmp[$tmpObj->getUniqueLabelField()])) {
                            $value = $tmp[$tmpObj->getUniqueLabelField()];
                        }
                        unset($tmpObj);
                    }

                    $value = str_replace("\n", "<br/>", $value);
                    $value = CentreonUtils::convertLineBreak($value);
                    echo $this->action . $this->delim . "setparam" . $this->delim . $element[$this->object->getUniqueLabelField()] . $this->delim . $this->getClapiActionName($parameter) . $this->delim . $value . "\n";
                }
            }

            $params = $this->object->getParameters($element[$this->object->getPrimaryKey()], array("icon_id"));
            if (is_array($params)) {
                foreach ($params as $k => $v) {
                    if (!is_null($v) && $v != "") {
                        $v = CentreonUtils::convertLineBreak($v);
                        echo $this->action . $this->delim . "setparam" . $this->delim . $element[$this->object->getUniqueLabelField()] . $this->delim . $this->getClapiActionName($k) . $this->delim . $v . "\n";
                    }
                }
            }
            
            $instanceRel = new \Centreon_Object_Relation_Instance_Ba();
            $instElements = $instanceRel->getMergedParameters(array("id", "name", "localhost"), array(), -1, 0, null, "ASC", array("mod_bam.name" => $element[$this->object->getUniqueLabelField()]), "AND");

            foreach ($instElements as $instElem) {
                if ($instElem['localhost'] != '1') {
                    $addStr = $this->action . $this->delim . "SETPOLLER".$this->delim 
                        . $element[$this->object->getUniqueLabelField()] . $this->delim
                        . $instElem['name'] . "\n";
                    echo $addStr;
                }
            }

        }
    }

    /**
     * Get clapi action name from db column name
     *
     * @param string $columnName
     * @return string
     */
    protected function getClapiActionName($columnName) {
        if (isset(self::$table[$columnName])) {
            return self::$table[$columnName];
        }
        return $columnName;
    }

    /**
     * validate thresholds
     *
     * @param int warning threshold
     * @param int critical threshold
     */
    protected function checkThresholds($warning, $critical) {
        if ($warning < $critical) {
            throw new \CentreonClapi\CentreonClapiException('Warning Threshold must be greater than Critical Threshold (0-100)');
        }
    }

    /**
     * Magic method
     *
     * @param string $name
     * @param array $args
     * @return void
     * @throws CentreonClapiException
     */
    public function __call($name, $arg)
    {
        $name = strtolower($name);
        if (!isset($arg[0])) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }
        $args = explode($this->delim, $arg[0]);
        $baIds = $this->object->getIdByParameter($this->object->getUniqueLabelField(), array($args[0]));
        if (!count($baIds)) {
            throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND .":".$args[0]);
        }
        $baId = $baIds[0];
        if (preg_match("/^(get|set|add|del)([a-zA-Z_]+)/", $name, $matches)) {
            if ($matches[1] == "add" && $matches[2] == "poller") {
                throw new \CentreonClapi\CentreonClapiException(self::UNKNOWN_METHOD);
            }
            switch ($matches[2]) {
                case "contactgroup":
                    $class = "Centreon_Object_Contact_Group";
                    $relclass = "Centreon_Object_Relation_Contact_Group_Ba";
                    break;
                case "bv":
                    $class = "Ba_Group";
                    $relclass = "Centreon_Object_Relation_Ba_Group_Ba";
                    break;
                case "poller":
                    $class = "Centreon_Object_Instance";
                    $relclass = "Centreon_Object_Relation_Instance_Ba";
                    break;
                case 'extrareportingperiod':
                    $class = "Centreon_Object_Timeperiod";
                    $relclass = "Centreon_Object_Relation_Timeperiod_Ba";
                    break;
                default:
                    throw new \CentreonClapi\CentreonClapiException(self::UNKNOWN_METHOD);
                    break;
            }
            if (class_exists($relclass) && class_exists($class) ) {
                $relobj = new $relclass();
                $obj = new $class();
                if ($matches[1] == "get") {
                    $tab = $relobj->getTargetIdFromSourceId($relobj->getFirstKey(), $relobj->getSecondKey(), $baId);
                    echo "id".$this->delim."name"."\n";
                    foreach($tab as $value) {
                        $tmp = $obj->getParameters($value, array($obj->getUniqueLabelField()));
                        echo $value . $this->delim . $tmp[$obj->getUniqueLabelField()] . "\n";
                    }
                } else {
                    if (!isset($args[1])) {
                        throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
                    }
                    $relations = explode("|", $args[1]);
                    $relationTable = array();
                    foreach($relations as $rel) {
                        $sRel = $rel;
                        if (is_string($rel)) {
                            $rel = htmlentities($rel, ENT_QUOTES, "UTF-8");
                        }
                        $tab = $obj->getIdByParameter($obj->getUniqueLabelField(), array($rel));
                        if (!count($tab)) {
                            throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND . ":".$sRel);
                        }
                        $relationTable[] = $tab[0];
                    }
                    if ($matches[1] == "set") {
                        $relobj->delete(null, $baId);
                    }
                    $existingRelationIds = $relobj->getTargetIdFromSourceId($relobj->getFirstKey(), $relobj->getSecondKey(), $baId);
                    foreach($relationTable as $relationId) {
                        if ($matches[1] == "del") {
                            $relobj->delete($relationId, $baId);
                        } else if ($matches[1] == "set" || $matches[1] == "add") {
                            if ($matches[1] == "set" && $matches[2] == "poller") {
                                $oKpi = new \CentreonBAM_Kpi($this->_DB);
                                $bCheck = $oKpi->instanceService($baId, $relationId);
                                if (!$bCheck) {
                                    throw new \CentreonClapi\CentreonClapiException(self::NOTSAMEPOLLER);
                                } else {
                                    $this->object->updatePoller($baId, $relationId);
                                }
                            } elseif (!in_array($relationId, $existingRelationIds)) {
                                if ($matches[2] == "poller") {
                                    $oKpi = new \CentreonBAM_Kpi($this->_DB);
                                    $bCheck = $oKpi->instanceService($baId, $relationId);
                                    if (!$bCheck) {
                                        throw new \CentreonClapi\CentreonClapiException(self::NOTSAMEPOLLER);
                                    } else {
                                        $relobj->insert($relationId, $baId);
                                    }
                                } else {
                                    $relobj->insert($relationId, $baId);
                                }
                            }
                        }
                    }
                }
            } else {
                throw new \CentreonClapi\CentreonClapiException(self::UNKNOWN_METHOD);
            }
        } else {
            throw new \CentreonClapi\CentreonClapiException(self::UNKNOWN_METHOD);
        }
    }
}
