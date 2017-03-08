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
require_once realpath(dirname(__FILE__)."/../../core/class/models/BooleanRule.php");

/**
 *
 * @author kevin duret <kduret@centreon.com>
 *
 */
class CentreonBooleanRule extends \CentreonClapi\CentreonObject {

    private $_DB;
    private $_DBC;
    private $centreon_path;
    const ORDER_UNIQUENAME = 0;
    const ORDER_EXPRESSION = 1;
    const ORDER_BOOL_STATE = 2;

    public static $aDepends = array(
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
        $this->object = new \BooleanRule();
        $this->action = 'BOOLEANRULE';
        $this->params = array(
            'activate' => '1'
        );
        
        $this->insertParams = array(
            'name', 'expression', 'bool_state'
        );

        $this->nbOfCompulsoryParams = count($this->insertParams);
        $this->exportExcludedParams = array_merge($this->insertParams, array($this->object->getPrimaryKey()));
    }

    /**
     * Display all boolean rules
     *
     * @param string $parameters
     */
    public function show($parameters = null)
    {
        $filters = array();
        if (isset($parameters)) {
            $filters = array($this->object->getUniqueLabelField() => "%".$parameters."%");
        }

        $params = array('boolean_id', 'name', 'expression', 'bool_state');
        $paramString = str_replace("boolean_", "", implode($this->delim, $params));
        echo $paramString . "\n";

        $elements = $this->object->getList($params, -1, 0, null, null, $filters);
        foreach ($elements as $tab) {
            echo implode($this->delim, $tab) . "\n";
        }
    }

    /**
     * Add a boolean rule
     *
     * @param string $parameters
     * @return void
     * @throws CentreonClapiException
     */
    public function add($parameters) 
    {
        $params = explode($this->delim, $parameters);
        if (count($params) < $this->nbOfCompulsoryParams) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }

        $addParams = array();
        $addParams[$this->object->getUniqueLabelField()] = $params[self::ORDER_UNIQUENAME];
        $addParams['expression'] = $params[self::ORDER_EXPRESSION];
        $addParams['bool_state'] = $params[self::ORDER_BOOL_STATE];

        $this->params = array_merge($this->params, $addParams);
        $this->checkParameters();
        parent::add();
    }

    /**
     * Set Parameters
     *
     * @param string $parameters
     * @return void
     * @throws Exception
     */
    public function setparam($parameters = null)
    {
        if (is_null($parameters)) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }

        $params = explode($this->delim, $parameters);
        if (count($params) < self::NB_UPDATE_PARAMS) {
            throw new \CentreonClapi\CentreonClapiException(self::MISSINGPARAMETER);
        }

        if (($objectId = $this->getObjectId($params[self::ORDER_UNIQUENAME])) != 0) {
            if ($params[1] == "activate" || $params[1] == "enable") {
                $params[1] = "activate";
            }
            $params[2]=str_replace("<br/>", "\n", $params[2]);
            $updateParams = array($params[1] => $params[2]);
            parent::setparam($objectId, $updateParams);
        } else {
            throw new \CentreonClapi\CentreonClapiException(self::OBJECT_NOT_FOUND.":".$params[self::ORDER_UNIQUENAME]);
        }
    }
}
