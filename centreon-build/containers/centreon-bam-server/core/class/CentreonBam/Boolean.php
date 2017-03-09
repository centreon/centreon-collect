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

require_once ("Kpi.php");

class CentreonBam_Boolean
{
    protected $db;
    protected $dbmon;
    
    /**
     * Constructor
     * 
     * @return void
     */
    public function __construct($db, $dbmon = null) {
        $this->db = $db;
        if (!is_null($dbmon)) {
            $this->dbmon = $dbmon;
        }
    }

    /**
     * Get boolean data
     *
     * @param int $booleanId
     * @param string $booleanName
     * @return array
     */
    public function getData($booleanId = null, $booleanName = null)
    {
        $data = array();

        $sql = "SELECT boolean_id, name, expression, bool_state, comments, activate
            FROM mod_bam_boolean";
        $res = $this->db->query($sql);
        while ($row = $res->fetchRow()) {
            $data[$row['boolean_id']] = $row;
            $data[$row['name']] = $row;
        }

        if (!is_null($booleanId) && isset($data[$booleanId])) {
            return $data[$booleanId];
        }

        if (!is_null($booleanName) && isset($data[$booleanName])) {
            return $data[$booleanName];
        }

        return array();
    }


    /**
     * Get boolean list
     *
     * @return array
     */
    public function getList()
    {
        $queryList = "SELECT `boolean_id`, `name`
                      FROM `mod_bam_boolean`
                      ORDER BY `name`";
        $res = $this->db->query($queryList);
        if (PEAR::isError($res)) {
            return array();
        }
        $listBool = array();
        while ($row = $res->fetchRow()) {
            $listBool[$row['boolean_id']] = $row['name'];
        }
        return $listBool;
    }


    /**
     * Check whether or not all compulsory parameters are there
     * 
     * @param string $paramString
     * @param array $params
     * @return bool
     */
    protected function hasAllParams($paramString, $params = array()) {
        $compulsory = explode(',', $paramString);
        foreach ($compulsory as $val) {
            if (!isset($params[$val])) {
                return false;
            }
        }
        return true;
    }
    
    /**
     * Insert method
     * 
     * @param array $params
     * @return int 
     * @throws Exception
     */
    public function insert($params = array()) {
        $paramString = 'name,expression,bool_state,comments,activate';
        if (false == $this->hasAllParams($paramString, $params)) {
            throw new Exception('Missing compulsory parameter');
        }
        if (is_array($params['bool_state'])) {
            $params['bool_state'] = $params['bool_state']['bool_state'];
        }
        if (is_array($params['activate'])) {
            $params['activate'] = $params['activate']['activate'];
        }
        $sql = "INSERT INTO mod_bam_boolean ($paramString) 
                VALUES (
                '".$this->db->escape($params['name'])."',
                '".$this->db->escape($params['expression'])."',
                '".$this->db->escape($params['bool_state'])."',
                '".$this->db->escape($params['comments'])."',
                '".$this->db->escape($params['activate'])."'
                )";
        $this->db->query($sql);
        $res = $this->db->query("SELECT MAX(boolean_id) as last_id FROM mod_bam_boolean WHERE name = '".$this->db->escape($params['name'])."'");
        $row = $res->fetchRow();

        return $row['last_id'];
    }
    
    /**
     * Update method
     * 
     * @param int $boolId
     * @param array $params
     * @return void
     */
    public function update($boolId, $params = array()) {
        $paramString = 'boolean_id,name,expression,bool_state,comments,activate';
        if (false == $this->hasAllParams($paramString, $params)) {
            throw new Exception('Missing compulsory parameter');
        }
        $sql = "UPDATE mod_bam_boolean SET ";
        $keys = explode(',', $paramString);
        $first = true;
        foreach ($keys as $k) {
            if ($first == false) {
                $sql .= ",";
            } else {
                $first = false;
            }
            if (is_array($params[$k])) {
                $value = $params[$k][$k];
                if ($k == 'activate') {
                    $activate = $value;
                }
            } else {
                $value = $params[$k];
                if ($k == 'activate') {
                    $activate = $value;
                }
            }
            $sql .= $k . " = '".$this->db->escape($value)."' ";
        }
        $sql .= "WHERE boolean_id = " . $this->db->escape($boolId);
        $this->db->query($sql);
        
        if (isset($params['activate'])) {
            $this->setStatusKpIByBolleanId($boolId, $activate);
        }
    }
    
    /**
     * Delete method
     * 
     * @param mixed $boolIds
     * @return void
     */
    public function delete($boolIds)
    {
        $arr = array();
        if (!is_array($boolIds)) {
            $arr = array($boolIds => 1);
        } else {
            $arr = $boolIds;
        }
        if (count($arr)) {
            $this->db->query("DELETE FROM mod_bam_boolean WHERE boolean_id IN (".implode(',', array_keys($arr)).")");
        }
    }

    /**
     * Duplicate boolean rule
     *
     * @param array $multiSelect
     */
    public function duplicate($multiSelect = null)
    {
        if (is_null($multiSelect)) {
            return null;
        }
        foreach ($multiSelect as $key => $value) {
            $boolConf = $this->getData($key);
            if (isset($boolConf) && isset($boolConf['boolean_id'])) {
                $nbDup = 0;
                if (isset($_POST['dup_'.$key]) && is_numeric($_POST['dup_'.$key])) {
                    $nbDup = $_POST['dup_'.$key];
                    unset($boolConf['boolean_id']);
                    for ($i = 0; $i < $nbDup; $i++) {
                        $copyConf = $boolConf;
                        $copyConf['name'] = $boolConf['name'] . '_' . ($i + 1);
                        $test = $this->getData(null, $copyConf['name']);
                        // already exists, loop until free slot is found
                        if (count($test)) {
                            $nbDup++;
                            continue;
                        }
                        $this->insert($copyConf);
                    }
                } 
            }
        }
    }

    /**
     * Enable / disable boolean kpi
     * 
     * @param array $boolIds
     * @param int $status 
     * @return void
     */
    public function setStatus($boolIds = array(), $status) {
        if (count($boolIds)) {
            $this->db->query("UPDATE mod_bam_boolean 
                              SET activate = '".$this->db->escape($status)."'
                              WHERE boolean_id IN (".implode(',', $boolIds).")");
            
            
            $query = "UPDATE `mod_bam_kpi` SET activate = '" . $this->db->escape($status). "' WHERE boolean_id IN (".implode(',', $boolIds).")";
            $this->db->query($query);
        
        }
    }
    
    /**
     * Enable boolean kpi
     * 
     * @param array $boolIds
     * @return void
     */
    public function enable($boolIds = array()) {
       $this->setStatus($boolIds, 1);
    }
    
    /**
     * Disable boolean kpi
     * 
     * @param array $boolIds
     * @return void
     */
    public function disable($boolIds = array()) {
       $this->setStatus($boolIds, 0); 
    }
    
    /**
     * Eval boolean expression
     * 
     * @param string $expression
     * @param array $statusParam
     * @return bool
     * @throws Exception
     */
    public function evalExpr($expression = "", $statusParam = array(), $evalStatus = true) {
        if (!$expression) {
            throw new Exception(_('Expecting expression'));
        }
        $expression = strtoupper($expression);
        $expression = str_replace("\r\n", " ", $expression);
        $expression = str_replace("\n", " ", $expression);
        $matches = array();
        preg_match_all("/{([^}]+)}/", $expression, $matches, PREG_SET_ORDER);
        $pattern = array();
        $replace = array();
        foreach ($matches as $match) {
            $pattern[] = "/{".preg_quote($match[1], "/")."}/";
            if (preg_match("/^(OK|WARNING|CRITICAL|UNKNOWN)$/", $match[1])) {
                $replace[] = $this->getMonitoringStateCode($match[1]);
            } elseif (preg_match("/^(NOT|IS)$/", $match[1])) {
                $replace[] = $this->getOperator($match[1]);
            } elseif (preg_match("/^(AND|OR|XOR)$/", $match[1])) {
                $replace[] = $this->getLogicalOperator($match[1]);
            } else {
                if (is_array($match)) {
                    $resourceStr = $match[1];
                } else {
                    $resourceStr = $match;
                }
                // Force status to OK
                if (!$evalStatus) {
                    $status = $this->checkResourceExistence($resourceStr);
                } elseif (!count($statusParam)) {
                    $this->checkResourceExistence($resourceStr);
                    $status = $this->getResourceStatus($resourceStr);
                } elseif (isset($statusParam[$match[1]])) {
                    $this->checkResourceExistence($resourceStr);
                    $status = $statusParam[$match[1]];
                } else {
                    throw new Exception(sprintf(_('Could not find status for %s', $match[1])));
                }
                $replace[] = $status;
                $this->resources[$match[1]] = $status;
            }
        }
        $final = preg_replace($pattern, $replace, $expression);
        $invalid = _('Invalid expression');
        if (!preg_match("/^[0-3=!\(\)\ &\|\^]+$/", $final)) {
            throw new Exception($invalid);
        }
        $result = @eval("return ($final) ? 1 : 0;");
        if (false === $result) {
            throw new Exception($invalid);
        }
        
        return $result ? true : false;
    }

    /**
     * Check if resource exists in configuration
     *
     * @param string resource name
     * @return int
     */
    protected function checkResourceExistence($str) {
        $tmp = explode(' ', $str);
        if (!isset($tmp[1])) {
            throw new Exception(sprintf(_('Service not specified for %s'), $tmp[0]));
        }
        $hostname = array_shift($tmp);
        $servicedesc = implode(' ', $tmp);

        $sql = "SELECT service_id "
            . "FROM host h, service s, host_service_relation hsr "
            . "WHERE h.host_name = '" . $this->db->escape($hostname) . "' "
            . "AND s.service_description = '" . $this->db->escape($servicedesc) . "' "
            . "AND h.host_id = hsr.host_host_id "
            . "AND s.service_id = hsr.service_service_id ";
        $res = $this->db->query($sql);
        
        if ($res->numRows()) {
            return 0;
        } else {
            throw new Exception(sprintf(_('Could not find resource %s'), $hostname . ' ' .$servicedesc));
        }
    }
    
    /**
     * Get operator
     * 
     * @param string $str
     * @return string 
     */
    protected function getOperator($str) {
        if ($str == 'IS') {
            return '==';
        }
        return '!=';
    }
    
    /**
     * Get logical operator
     * 
     * @param string $str
     * @return string
     */
    protected function getLogicalOperator($str) {
        if ($str == 'AND') {
            return '&&';
        } elseif ($str == 'XOR') {
            return '^';
        }
        return '||';
    }
    
    /**
     * Get monitoring state code
     * 
     * @param string $str
     * @return int
     */
    protected function getMonitoringStateCode($str) {
        if ($str == 'OK') {
            return 0;
        } elseif ($str == 'CRITICAL') {
            return 2;
        } elseif ($str == 'WARNING') {
            return 1;
        }
        return 3;
    }

    /**
     * Get status query
     * 
     * @param string $hostname
     * @param string $servicedesc
     * @return string
     */
    protected function getStatusQuery($hostname, $servicedesc) {
        $sql = "SELECT s.state as current_state
                FROM services s, hosts h
                WHERE s.host_id = h.host_id
                AND s.description = '".$servicedesc."' 
                AND h.name = '".$hostname."'
                AND s.enabled = 1";
        return $sql;
    }
    
    /**
     * Get resource status
     * 
     * @param string $str
     * @return int
     * @throws Exception
     * @todo dynamic ndo prefix!!!
     */
    protected function getResourceStatus($str) {
        $tmp = explode(' ', $str);
        if (!isset($tmp[1])) {
            throw new Exception(sprintf(_('Service not specified for %s'), $tmp[0]));
        }
        $hostname = array_shift($tmp);
        $servicedesc = implode(' ', $tmp);
        $sql = $this->getStatusQuery($hostname, $servicedesc);
        $res = $this->dbmon->query($sql);
        if ($res->numRows()) {
            $row = $res->fetchRow();
            return $row['current_state'];
        } else {
            throw new Exception(sprintf(_('Could not find status for %s'), $hostname . ' ' .$servicedesc));
        }
    }
    
    /**
     * Get resource list
     * 
     * @return array
     */
    public function getResources() {
        if (isset($this->resources)) {
            return $this->resources;
        }
        return array();
    }
    
    /**
     * 
     * @param int $iIdBoolean
     * @param int $iStatus
     */
    public function setStatusKpIByBolleanId($iIdBoolean, $iStatus)
    {
        if (!is_numeric($iIdBoolean) || !is_numeric($iStatus)) {
            return;
        }
        $query = "UPDATE `mod_bam_kpi` SET activate = '" . $this->db->escape($iStatus) . "' WHERE boolean_id = '".$this->db->escape($iIdBoolean)."'";
        $this->db->query($query);
    }
    
    /**
     * 
     * @param string $sExpression
     * @param string $sSearch
     * @return array
     * @throws Exception
     */
    public function explodeExpression($sExpression, $sSearch = '')
    {
        if (empty($sExpression)) {
            throw new Exception(_('Expecting expression'));
        }
        $aReponse   = array();
        $aStatus    = array();
        $aObject    = array();
        $aLogical   = array();
        $aCondition = array();
        $matches = array();
        
        $sExpression = strtoupper($sExpression);
        $sExpression = str_replace("\r\n", " ", $sExpression);
        $sExpression = str_replace("\n", " ", $sExpression);
        
        preg_match_all("/{([^}]+)}/", $sExpression, $matches, PREG_SET_ORDER);

        foreach ($matches as $match) {
            $pattern[] = "/{".preg_quote($match[1], "/")."}/";
            if (preg_match("/^(OK|WARNING|CRITICAL|UNKNOWN)$/", $match[1])) {
                $aStatus[] = $match[1];
            } elseif (preg_match("/^(NOT|IS)$/", $match[1])) {
                $aCondition[] = $match[1];
            } elseif (preg_match("/^(AND|OR|XOR)$/", $match[1])) {
                $aLogical[] = $match[1];
            } else {
                $aObject[] = $match[1];
            }
        }
        $aReponse['object']    = array_unique($aObject);
        $aReponse['status']    = array_unique($aStatus);
        $aReponse['logical']   = array_unique($aLogical);
        $aReponse['condition'] = array_unique($aCondition);
 
        if (!empty($sSearch) && isset($aReponse[$sSearch])) {
            return $aReponse[$sSearch];
        } else {
            return $aReponse;
        }
    }
}
