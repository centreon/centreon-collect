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
class CentreonBam_Command
{
	protected $_db;
	
	/*
	 *  Constructor
	 */
	function __construct($db)
	{
		$this->_db = $db;
	}

	/*
	 * Return Id from timeperiod alias
	 */
	function getCommandId($name = null)
	{
		if (!isset($name)) {
			return null;
		}
		$query = "SELECT command_id FROM command WHERE command_name = '".$name."' LIMIT 1";
		$res = $this->_db->query($query);
		if ($res->numRows()) {
			$row = $res->fetchRow();			
			return ($row['command_id']);	
		}
		return null;
	}	
}