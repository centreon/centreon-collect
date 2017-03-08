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

require_once _CENTREON_PATH_ . 'www/modules/centreon-bam-server/core/class/Centreon/Utils.php';

/*
 * Class that contains all methods for managing ACL
 */
class CentreonBam_Acl
{
	protected $_db;
	protected $_userId;
	protected $_accessGroup = array();
	protected $_accessGroupStr = "";
	protected $_admin;
	protected $_baGroup = array();
	protected $_baList = array();
	protected $_baListStr = "";
	protected $_baGroup_str = "";

	/*
	 * Constructor
	 */
	public function __construct($db, $userId)
	{
		$this->_db = $db;
		$this->_userId = $userId;
		$this->_setAdmin();
		$this->_setAccessGroups();
		$this->_setBAGroup();
		$this->_setBA();
	}

	/*
	 *  Private set admin
	 */
	protected function _setAdmin()
	{
		$query = "SELECT contact_admin FROM `contact` WHERE contact_id = '".$this->_userId."' LIMIT 1";
		$res = $this->_db->query($query);
		if (!$res->numRows()) {
			$this->_admin = 0;
		} else {
			$row = $res->fetchRow();
			$this->_admin = $row['contact_admin'];
		}
	}

	/**
	 * Sets access groups
	 *
	 * @return void
	 */
	protected function _setAccessGroups()
	{
		$query = "SELECT acl_group_id
				  FROM `acl_group_contacts_relations`
				  WHERE contact_contact_id = '" .$this->_userId. "'";
		$res = $this->_db->query($query);
		while ($row = $res->fetchRow()) {
			if ($this->_accessGroupStr != "") {
				$this->_accessGroupStr .= ", ";
			}
			$this->_accessGroupStr .= "'" . $row['acl_group_id'] . "'";
			$this->_accessGroup[$row['acl_group_id']] = 1;
		}
		if (CentreonBam_Utils::tableExists($this->_db, "acl_group_contactgroups_relations")) {
            $query2 = "SELECT acl_group_id
            		   FROM `acl_group_contactgroups_relations`
            		   WHERE cg_cg_id IN (SELECT contactgroup_cg_id
            		   					  FROM `contactgroup_contact_relation`
            		   					  WHERE contact_contact_id = ".$this->_userId.")";
            $res2 = $this->_db->query($query2);
		    while ($row2 = $res2->fetchRow()) {
			    if ($this->_accessGroupStr != "") {
				    $this->_accessGroupStr .= ", ";
			    }
			    $this->_accessGroupStr .= "'" . $row2['acl_group_id'] . "'";
			    $this->_accessGroup[$row2['acl_group_id']] = 1;
		    }
		}
		if ($this->_accessGroupStr == "") {
			$this->_accessGroupStr = "''";
		}
	}

	/*
	 *  Sets groups of BA
	 */
	protected function _setBAGroup()
	{
		$query = "SELECT ba_group_id FROM `mod_bam_acl` ";
                if (!$this->_admin) {
                    $query .= "WHERE acl_group_id IN (".$this->getAccessGroupsStr().")";
                }
		$res = $this->_db->query($query);
		while ($row = $res->fetchRow()) {
			if ($this->_baGroup_str != "") {
				$this->_baGroup_str .= ", ";
			}
			$this->_baGroup_str .= "'" . $row['ba_group_id'] . "'";
			$this->_baGroup[$row['ba_group_id']] = 1;
		}
		if ($this->_baGroup_str == "") {
			$this->_baGroup_str = "''";
		}
	}

	/*
	 *  Sets BA
	 */
	protected function _setBA()
	{
            if ($this->_admin) {
                $query = "SELECT ba_id as id_ba from mod_bam";
            } else {
		$query = "SELECT id_ba FROM `mod_bam_bagroup_ba_relation` WHERE id_ba_group IN (".$this->getBaGroupStr().")";
            }
		$res = $this->_db->query($query);
		while ($row = $res->fetchRow()) {
			if ($this->_baListStr != "") {
				$this->_baListStr .= ", ";
			}
			$this->_baListStr .= "'" . $row['id_ba'] . "'";
			$this->_baList[$row['id_ba']] = 1;
		}
		if ($this->_baListStr == "") {
			$this->_baListStr = "''";
		}
	}

	/*
	 *  checks if user can access a page
	 */
	public function page($p)
	{
		if ($this->_admin) {
			return 1;
		}
		$query = "SELECT * ".
				"FROM acl_group_contacts_relations agcr, acl_group_topology_relations agtr, acl_topology_relations atr, topology t " .
				"WHERE agcr.contact_contact_id = '".$this->_userId."' ".
				"AND agcr.acl_group_id = agtr.acl_group_id " .
				"AND agtr.acl_topology_id = atr.acl_topo_id " .
				"AND atr.topology_topology_id = t.topology_id " .
				"AND t.topology_page = '".$p."'";
		$res = $this->_db->query($query);
		return $res->numRows();
	}

	/*
	 *  Gets access group array
	 */
	public function getAccessGroups()
	{
		return $this->_accessGroup;
	}

	/*
	 *  Gets access group str
	 */
	public function getAccessGroupsStr()
	{
		return $this->_accessGroupStr;
	}

	/*
	 *  Get BA Group Array
	 */
	public function getBaGroup()
	{
		return $this->_baGroup;
	}

	/*
	 *  Get BA Group Str
	 */
	public function getBaGroupStr()
	{
		return $this->_baGroup_str;
	}

	/*
	 * Get BA array
	 */
	public function getBa()
	{
		return $this->_baList;
	}

	/*
	 *  Get list of BA
	 */
	public function getBaStr()
	{
		return $this->_baListStr;
	}

	/*
	 *  Query builder
	 */
	 public function queryBuilder($cond, $cmp, $list)
	 {
	 	if ($this->_admin) {
	 		return "";
	 	}
	 	$str = $cond . " " . $cmp . " IN (" . $list . ") ";
	 	return $str;
	 }
}
