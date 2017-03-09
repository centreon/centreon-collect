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
require_once realpath(dirname(__FILE__)."/../../core/class/models/Ba.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/BaGroup.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/Relation/Aclgroup/Bagroup.php");
require_once realpath(dirname(__FILE__)."/../../core/class/models/Relation/Ba/Bagroup.php");
require_once "Centreon/Object/Graph/Template/Template.php";
require_once "Centreon/Object/Acl/Group.php";

/**
 *
 * @author kevin duret <kduret@centreon.com>
 *
 */
class CentreonBv extends \CentreonClapi\CentreonObject {

    private $_DB;
    private $_DBC;
    private $centreon_path;

    const ORDER_UNIQUENAME = 0;
    const ORDER_DESCRIPTION = 1;

    public static $aDepends = array(
        'BA'
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
        $this->object = new \Ba_Group();

        $this->params = array(
            'visible' => '1'
        );
        $this->action = "BV";
        $this->insertParams = array('ba_group_name', 'ba_group_description');
        $this->exportExcludedParams = array_merge($this->insertParams, array($this->object->getPrimaryKey()));
        $this->nbOfCompulsoryParams = count($this->insertParams);
    }

    /**
     * Add a business view
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
        $addParams['ba_group_description'] = $params[self::ORDER_DESCRIPTION];
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
        $params = array('id_ba_group', 'ba_group_name', 'ba_group_description');
        $paramString = str_replace("ba_group_", "", implode($this->delim, $params));
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
                case 'overview':
                    $params[1] = 'visible';
                    break;
                case 'name':
                    $params[1] = 'ba_group_name';
                    break;
                case 'description':
                    $params[1] = 'ba_group_description';
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
        $elements = $this->object->getList("*", -1, 0);
        foreach ($elements as $element) {
            $addStr = $this->action.$this->delim."ADD";
            foreach ($this->insertParams as $param) {
                $addStr .= $this->delim.$element[$param];
            }
            $addStr .= "\n";
            echo $addStr;

            foreach ($element as $parameter => $value) {
                if (!in_array($parameter, $this->exportExcludedParams)) {
                    if (!is_null($value) && $value != "") {
                        $value = CentreonUtils::convertLineBreak($value);
                        echo $this->action.$this->delim."setparam".$this->delim.$element[$this->object->getUniqueLabelField()].$this->delim.$parameter.$this->delim.$value."\n";
                    }
                }
            }

            $objects = array(
                array(
                    'obj' => 'BA',
                    'relObj' => 'Centreon_Object_Relation_Ba_Ba_Group',
                    'action' => 'addba'
                )
            );

            foreach ($objects as $object) {
                $obj = new $object['obj']();
                $relObj = new $object['relObj']();
                foreach ($element as $parameter => $value) {
                    if ($parameter == $this->object->getPrimaryKey()) {
                        $tab = $relObj->getTargetIdFromSourceId($relObj->getFirstKey(), $relObj->getSecondKey(), $value);
                        foreach($tab as $subvalue) {
                            $tmp = $obj->getParameters($subvalue, array($obj->getUniqueLabelField()));
                            echo $this->action.$this->delim.$object['action'].$this->delim.$element[$this->object->getUniqueLabelField()].$this->delim.$tmp[$obj->getUniqueLabelField()]."\n";
                        }
                    }
                }
            }
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
        $bvIds = $this->object->getIdByParameter($this->object->getUniqueLabelField(), array($args[0]));
        if (!count($bvIds)) {
            throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND .":".$args[0]);
        }
        $bvId = $bvIds[0];
        if (preg_match("/^(get|set|add|del)([a-zA-Z_]+)/", $name, $matches)) {
            switch ($matches[2]) {
                case "ba":
                    $class = "\\Ba";
                    $relclass = "\\Centreon_Object_Relation_Ba_Ba_Group";
                    break;
                case "aclgroup":
                    $class = "\\Centreon_Object_Acl_Group";
                    $relclass = "\\Centreon_Object_Relation_Acl_Group_Ba_Group";
                    break;
                default:
                    throw new \CentreonClapi\CentreonClapiException(self::UNKNOWN_METHOD);
                    break;
            }
            if (class_exists($relclass) && class_exists($class)) {
                $relobj = new $relclass();
                $obj = new $class();
                if ($matches[1] == "get") {
                    $tab = $relobj->getTargetIdFromSourceId($relobj->getFirstKey(), $relobj->getSecondKey(), $bvId);
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
                        $tab = $obj->getIdByParameter($obj->getUniqueLabelField(), array($rel));
                        if (!count($tab)) {
                            throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND . ":".$rel);
                        }
                        $relationTable[] = $tab[0];
                    }
                    if ($matches[1] == "set") {
                        $relobj->delete(null, $bvId);
                    }
                    $existingRelationIds = $relobj->getTargetIdFromSourceId($relobj->getFirstKey(), $relobj->getSecondKey(), $bvId);
                    foreach($relationTable as $relationId) {
                        if ($matches[1] == "del") {
                            $relobj->delete($relationId, $bvId);
                        } else if ($matches[1] == "set" || $matches[1] == "add") {
                            if (!in_array($relationId, $existingRelationIds)) {
                                $relobj->insert($relationId, $bvId);
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
