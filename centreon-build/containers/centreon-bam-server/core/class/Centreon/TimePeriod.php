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
 *  Class that manages timeperiods
 */
class CentreonBam_TimePeriod
{
	protected $_db;
	
	/*
	 *  Constructor
	 */
	function __construct($db)
	{
		$this->_db = $db;
	}
	 
	/**
     * Get the time period of the current day!
     * @param int $tpId
     * @return type
     */
	function getTodayTimePeriod($tpId = null)
	{
		if (!isset($tpId)) {
			return null;
		}
		$today = trim(strtolower(date("l", time())));		
		$query = "SELECT * FROM timeperiod WHERE tp_id = '".$tpId."' LIMIT 1";
		$res = $this->_db->query($query);
		if ($res->numRows()) {
			$row = $res->fetchRow();
			if (isset($row['tp_'.$today])) {
				return ($row['tp_'.$today]);
			}
			return null;
		}
		return null;
	}
	
	/*
	 *  Get the time period of the current day!
	 */
	function getTimePeriodName($tpId = null)
	{
		if (!isset($tpId)) {
			return null;
		}

		$query = "SELECT tp_name FROM timeperiod WHERE tp_id = '".$tpId."' LIMIT 1";
		$res = $this->_db->query($query);
		if ($res->numRows()) {
			$row = $res->fetchRow();
			if (isset($row['tp_name'])) {
				return ($row['tp_name']);
			}
			return null;
		}
		return null;
	}

	/*
	 * Return Id from timeperiod alias
	 */
	function getTimePeriodId($alias = null)
	{
		if (!isset($alias)) {
			return null;
		}
		$query = "SELECT * FROM timeperiod WHERE tp_alias = '".$alias."' LIMIT 1";
		$res = $this->_db->query($query);
		if ($res->numRows()) {
			$row = $res->fetchRow();			
			return ($row['tp_id']);	
		}
		return null;
	}	
}