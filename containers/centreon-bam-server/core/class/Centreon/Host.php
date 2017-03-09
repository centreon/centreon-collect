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
  *  Class that contains various methods for managing hosts 
  */
 class CentreonBam_Host
 {
 	protected $_db;
        protected $_cacheName;
 	protected $_cacheAlias;
 	protected $_cacheAddress;
 	
 	/*
 	 *  Constructor
 	 */
 	function __construct($db)
 	{
 		$this->_db = $db;
 		$this->_cacheName = array();
 		$this->_cacheAlias = array();
 		$this->_cacheAddress = array();
 		$this->_createCache();
 	}
 	
 	/*
 	 * Create cache for host names 
 	 */
 	protected function _createCache()
 	{ 	    
        $rq = "SELECT DISTINCT h.host_id, h.host_name, h.host_alias, h.host_address 
             FROM host h, mod_bam_kpi kpi
             WHERE h.host_id = kpi.host_id";
 	    $res = $this->_db->query($rq);
 	    while ($row = $res->fetchRow()) {
 	        $this->_cacheName[$row['host_id']] = $row['host_name'];
 	        $this->_cacheAlias[$row['host_id']] = $row['host_alias'];
 	        $this->_cacheAddress[$row['host_id']] = $row['host_address'];
 	    }
 	}

    /**
     * Insert host cache
     * 
     * @param int $hostId
     */ 
    protected function _insertHostCache($hostId)
    {
        $sql = "SELECT host_id, host_name, host_alias, host_address 
            FROM host 
            WHERE host_id = " . $this->_db->escape($hostId);
        $res = $this->_db->query($sql);
        if ($res->numRows()) {
              $row = $res->fetchRow();
              $this->_cacheName[$row['host_id']] = $row['host_name'];
       	      $this->_cacheAlias[$row['host_id']] = $row['host_alias'];
    	      $this->_cacheAddress[$row['host_id']] = $row['host_address'];
        }
    }

 	/*
 	 *  Method that returns a hostname from hostId
 	 */
 	public function getHostName($hostId)
    {
        if (!isset($this->_cacheName[$hostId])) { 
            $this->_insertHostCache($hostId);
        }
 		if (isset($this->_cacheName[$hostId])) {
 		    return $this->_cacheName[$hostId];
        }
 		return null;
 	} 
 	 	
 	/*
 	 *  Method that returns a host alias from host_id
 	 */
 	public function getHostAlias($hostId)
    {
        if (!isset($this->_cacheAlias[$hostId])) {
            $this->_insertHostCache($hostId);
        }
 		if (isset($this->_cacheAlias[$hostId])) {
 		    return $this->_cacheAlias[$hostId];
 		}
 		return null;
 	}
 	
 	/*
 	 *  Method that returns a host address from host_id
 	 */
 	public function getHostAddress($hostId)
 	{
        if (!isset($this->_cacheAddress[$hostId])) {
            $this->_insertHostCache($hostId);
        }
        if (isset($this->_cacheAddress[$hostId])) {
 		    return $this->_cacheAddress[$hostId];
 		}
 		return null;
 	}
 	
 	/*
 	 *  Method that returns an id from from host_name
 	 */
 	public function getHostId($hostName)
 	{
 		$hname = str_replace("/", "#S#", $hostName);
 		$hname = str_replace("\\", "#BS#", $hname);
 		$query = "SELECT host_id FROM `host` WHERE host_name = '".$hname."' LIMIT 1";
 		$res = $this->_db->query($query);
 		if (!$res->numRows()) {
 			return null;
 		}
 		$row = $res->fetchRow();
 		return $row['host_id'];
 	}
 	
 	/*
 	 *  Returns a string that replaces on demand macros by their values
 	 */
 	public function replaceMacroInString($hostId, $string)
 	{ 		 		
 		if (preg_match("/$HOSTADDRESS$/", $string)) {
 			$string = str_replace("\$HOSTADDRESS\$", $this->getHostAddress($hostId), $string);
 		}
 		if (preg_match("/$HOSTNAME$/", $string)) {
 			$string = str_replace("\$HOSTNAME\$", $this->getHostName($hostId), $string);
 		}
 		if (preg_match("/$HOSTALIAS$/", $string)) {
 			$string = str_replace("\$HOSTALIAS\$", $this->getHostAlias($hostId), $string);
 		}
 		$matches = array();
 		$pattern = '|(\$_HOST[0-9a-zA-Z]+\$)|';
 		preg_match_all($pattern, $string, $matches);
 		$i = 0;
 		while (isset($matches[1][$i])) {	 			 			
 			$rq = "SELECT host_macro_value FROM on_demand_macro_host WHERE host_host_id = '".$hostId."' AND host_macro_name LIKE '".$matches[1][$i]."'"; 			
 			$res = $this->_db->query($rq); 			 			
	 		while ($row = $res->fetchRow()) {
	 			$string = str_replace($matches[1][$i], $row['host_macro_value'], $string);
	 		} 			 			 			
 			$i++;
 		}
 		if ($i) {
	 		$rq2 = "SELECT host_tpl_id FROM host_template_relation WHERE host_host_id = '".$hostId."' ORDER BY `order`";
	 		$res2 = $this->_db->query($rq2);
	 		while ($row2 = $res2->fetchRow()) {
	 			$string = $this->replaceMacroInString($row2['host_tpl_id'], $string);
	 		}
 		}
 		return $string;
 	}
        
        /**
         * Return list of hosts
         * 
         * @return array
         */
        public function getHostNames() {
            $res = $this->_db->query("SELECT host_name FROM host WHERE host_register = '1' ORDER BY host_name");
            $list = array();
            while ($row = $res->fetchRow()) {
                $list[$row['host_name']] = $row['host_name'];
            }
            return $list;
        }
}
