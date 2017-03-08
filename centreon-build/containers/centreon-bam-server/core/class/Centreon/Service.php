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
  *  Class that contains various methods for managing services
  */
 class CentreonBam_Service
 {
 	protected $_db;
 	protected $_cacheDescription;

 	/*
 	 *  Constructor
 	 */
 	function __construct($db)
 	{
 		$this->_db = $db;
 		$this->_cacheDescription = array();
 		$this->_createCache();
 	}

 	/*
 	 *  Creates cache
 	 */
 	protected function _createCache()
 	{
  	    $rq = "SELECT DISTINCT s.service_id, s.service_description 
			 FROM service s, mod_bam_kpi kpi
			 WHERE s.service_id = kpi.service_id";
 	    $res = $this->_db->query($rq);
 	    while ($row = $res->fetchRow()) {
 	        $this->_cacheDescription[$row['service_id']] = $row['service_description'];
 	    }
	}

    /**
     * Insert host cache
     * 
     * @param int $serviceId
     */ 
    protected function _insertServiceCache($serviceId)
    {
        $sql = "SELECT service_id, service_description 
            FROM service
            WHERE service_id = " . $this->_db->escape($serviceId);
        $res = $this->_db->query($sql);
        if ($res->numRows()) {
              $row = $res->fetchRow();
              $this->_cacheDescription[$row['service_id']] = $row['service_description'];
        }
	}

 	/*
 	 *  Method that returns a host address from host_id
 	 */
 	public function getServiceDesc($svcId)
	{
   	    if (!isset($this->_cacheDescription[$svcId])) {
            $this->_insertServiceCache($svcId);
		}
 		if (isset($this->_cacheDescription[$svcId])) {
 		    return $this->_cacheDescription[$svcId];
 		}
 	    return null;
 	}

 	/*
 	 *  Method that returns an id from service_description
 	 */
 	public function getServiceId($svcDesc, $hostName)
 	{
 		$sdesc = str_replace("/", "#S#", $svcDesc);
 		$sdesc = str_replace("\\", "#BS#", $sdesc);
 		$query = "SELECT s.service_id " .
 				"FROM `service` s, `host` h, `host_service_relation` hsr " .
 				"WHERE (s.service_description = '".$this->_db->escape($sdesc)."'
 				OR s.service_description = '".$this->_db->escape($svcDesc)."') " .
 				"AND s.service_id = hsr.service_service_id " .
 				"AND hsr.host_host_id = h.host_id AND " .
 				"h.host_name = '".$hostName."' " .
 				"LIMIT 1";
 		$res = $this->_db->query($query);
 		if (!$res->numRows()) {
 			return null;
 		}
 		$row = $res->fetchRow();
 		return $row['service_id'];
 	}

 	/*
 	 *  Returns a string that replaces on demand macros by their values
 	 */
 	public function replaceMacroInString($svcId, $string)
 	{
 		if (preg_match("/$SERVICEDESC$/", $string)) {
 			$string = str_replace("\$SERVICEDESC\$", $this->getServiceDesc($svcId), $string);
 		}
 		$matches = array();
 		$pattern = '|(\$_SERVICE[0-9a-zA-Z]+\$)|';
 		preg_match_all($pattern, $string, $matches);
 		$i = 0;
 		while (isset($matches[1][$i])) {
 			$rq = "SELECT svc_macro_value FROM on_demand_macro_service WHERE svc_svc_id = '".$svcId."' AND svc_macro_name LIKE '".$matches[1][$i]."'";
 			$res =& $this->_db->query($rq);
	 		while ($row = $res->fetchRow()) {
	 			$string = str_replace($matches[1][$i], $row['svc_macro_value'], $string);
	 		}
 			$i++;
 		}
 		if ($i) {
	 		$rq2 = "SELECT service_template_model_stm_id FROM service WHERE service_id = '".$svcId."'";
	 		$res2 = $this->_db->query($rq2);
	 		while ($row2 = $res2->fetchRow()) {
	 			$string = $this->replaceMacroInString($row2['service_template_model_stm_id'], $string);
	 		}
 		}
 		return $string;
 	}
        
        /**
         * Return list of services
         * 
         * @return array
         */
        public function getServiceNames() {
            $res = $this->_db->query("SELECT DISTINCT(service_description) FROM service WHERE service_register = '1' ORDER BY service_description");
            $list = array();
            while ($row = $res->fetchRow()) {
                $list[$row['service_description']] = $row['service_description'];
            }
            return $list;
        }
}
