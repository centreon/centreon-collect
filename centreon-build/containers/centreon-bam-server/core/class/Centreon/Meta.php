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
 class CentreonBam_Meta
 {
   	protected $_db;
  	protected $_cacheName;
  	protected $_cacheDescription;

   	/*
   	 *  Constructor
   	 */
   	function __construct($db)
   	{
   		$this->_db = $db;
   		$this->_cacheName = array();
   		$this->_cacheDescription = array();
   		$this->_createCache();
   	}

   	/*
   	 *  Create cache
   	 */
   	protected function _createCache()
   	{
   	    $rq = "SELECT meta_id, meta_name, meta_display FROM meta_service";
   	    $res = $this->_db->query($rq);
   	    while ($row = $res->fetchRow()) {
   	        $this->_cacheName[$row['meta_id']] = $row['meta_name'];
   	        $this->_cacheDescription[$row['meta_id']] = $row['meta_display'];
   	    }
   	}

   	/*
   	 *  Method that returns a metaname from meta_id
   	 */
   	public function getMetaName($metaId)
   	{
   		if (isset($this->_cacheName[$metaId])) {
   		    return $this->_cacheName[$metaId];
   		}
   		return null;
   	}

   	/*
    	 *  Method that returns a meta desc from meta_id
   	 */
   	public function getMetaDesc($metaId)
   	{
   		  if (isset($this->_cacheDescription[$metaId])) {
   		      return $this->_cacheDescription[$metaId];
   		  }
   		  return null;
   	}

  	/*
  	 *  Method that retuns an id from meta name
  	 */
  	public function getMetaId($metaName)
  	{
   		$query = "SELECT meta_id FROM `meta_service` WHERE meta_name = '".$metaName."' LIMIT 1";
   		$res = $this->_db->query($query);
   		if (!$res->numRows()) {
   			  return null;
   		}
   		$row = $res->fetchRow();
   		return $row['meta_id'];
  	}

  	/**
  	 * Get Centreon Meta Id
  	 *
  	 * @param CentreonBam_Db $dbc
  	 * @return int
  	 */
  	public function getCentreonHostMetaId($dbc)
  	{
          static $hostId = null;

          if (is_null($hostId)) {
              $query = "SELECT host_id FROM hosts WHERE name = '_Module_Meta' LIMIT 1";
              $res = $dbc->query($query);
              if ($res->numRows()) {
                  $row = $res->fetchRow();
                  $hostId  = $row['host_id'];
              } else {
                  $hostId = 0;
              }
          }
  	    return $hostId;
  	}

  	/**
  	 * Get Centreon Service Ba Id
  	 *
  	 * @param int $baId
  	 * @param CentreonBam_Db $dbc
  	 * @return int
  	 */
  	public function getCentreonServiceMetaId($metaId, $dbc)
  	{
          static $metaTab = null;

          if (is_null($metaTab)) {
               $metaTab = array();
               $query = "SELECT service_id,
                                description
                         FROM services s, hosts h
                         WHERE s.host_id = h.host_id
                         AND s.description LIKE 'meta_%'
                         AND h.host_id = " . $this->getCentreonHostMetaId($dbc);
               $res = $dbc->query($query);
               if ($res->numRows()) {
                   while ($row = $res->fetchRow()) {
                       $tmp = explode("_", $row['description']);
                       $mId = 0;
                       if (isset($tmp[1])) {
                           $mId = $tmp[1];
                       }
                       $metaTab[$mId] = $row['service_id'];
                   }
               }
          }
          if (isset($metaTab[$metaId])) {
              return $metaTab[$metaId];
          }
          return 0;
  	}
}
