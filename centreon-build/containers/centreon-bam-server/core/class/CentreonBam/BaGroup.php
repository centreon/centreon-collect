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
 * Class that contains all methods for managing BA Groups
 */
class CentreonBam_BaGroup
{
    /**
     *
     * @var CentreonDB 
     */
	protected $_db;
    
    /**
     *
     * @var type 
     */
	protected $_form;

	/**
     * Constructor
     * @param CentreonDB $db
     * @param type $form
     */
	function __construct($db, $form = null)
	{
		$this->_db = $db;
		$this->_form = null;
		if (isset($form)) {
			$this->_form = $form;
		}
	}

	/**
     * Checks if Group already exists if it exists, function returns false otherwise, it returns true
     * @param string $name
     * @param int $baGroupId
     * @return boolean
     */
	function testBAGroupExistence($name, $baGroupId = null)
	{
		$query = "SELECT * FROM mod_bam_ba_groups WHERE ba_group_name = '".$name."'";
		if (isset($baGroupId)) {
			$query .= " AND id_ba_group != '".$baGroupId."'";
		}
		$res = $this->_db->query($query);
		if ($res->numRows()) {
			return false;
		}
		return true;
	}

	/**
     * Inserts a BA group into database
     * @param array $conf
     * @param int $dupIndex
     */
	public function insertBAGroupInDB($conf = null, $dupIndex = null)
	{
		if (isset($_POST['ba_group_name'])) {
			$ba_group_name = $_POST['ba_group_name'];
			$ba_group_desc = $_POST['ba_group_desc'];
			$ba_group_display = $_POST['ba_group_display']['ba_group_display'];
		} elseif (isset($conf)) {
			$ba_group_name = $conf['ba_group_name'] . "_" . $dupIndex;
			$ba_group_desc = $conf['ba_group_description'];
			$ba_group_display = $conf['visible'];
		}

		if ($this->testBAGroupExistence($ba_group_name)) {
			$query = "INSERT INTO `mod_bam_ba_groups` " .
					"(`ba_group_name`, `ba_group_description`, `visible`) " .
					"VALUES ('".$ba_group_name."', '".$ba_group_desc."', '".$ba_group_display."')";
			$this->_db->query($query);
			$query2 = "SELECT MAX(id_ba_group) FROM `mod_bam_ba_groups` LIMIT 1";
			$res = $this->_db->query($query2);
			$row = $res->fetchRow();
			$ba_g_id = $row['MAX(id_ba_group)'];
            
            // Create Acl Resource
            $bvAclResourceName = 'HiddenBv_' . $ba_g_id . '_DoNotDelete';
            $queryInsertAclResource = "INSERT INTO "
                . "`acl_resources`(`acl_res_name`, `acl_res_alias`, `acl_res_status`, `locked`, `changed`) "
                . "VALUES('$bvAclResourceName', '$bvAclResourceName', '1', 1, 1)";
            $this->_db->query($queryInsertAclResource);
            
            // Update linked object
			$this->updateBA_ACL($ba_g_id, $conf);
			$this->updateBA_List($ba_g_id, $conf);
		}
	}

	/**
     * Updates a BA group
     * @param int $idBaGroup
     */
	public function updateBAGroup($idBaGroup)
	{
		if ($this->testBAGroupExistence($_POST['ba_group_name'], $idBaGroup)) {
			$query = "UPDATE `mod_bam_ba_groups` " .
					"SET ba_group_name = '" . $_POST["ba_group_name"] . "', " .
					"ba_group_description = '" . $_POST["ba_group_desc"] . "', " .
					"visible = '" . $_POST["ba_group_display"]["ba_group_display"] . "' "  .
					"WHERE id_ba_group = '".$idBaGroup."'";
            
			$this->_db->query($query);
			$this->updateBA_ACL($idBaGroup);
			$this->updateBA_List($idBaGroup);
		}
	}
    
    /**
     * Returns ba groups list
     * @param array $filters
     * @param string $aclFilter
     * @return array
     */
    public function getBaGroupList($filters = array(), $aclFilter)
    {
        $customFilter = '';
        $baGroupList = array();
        
        foreach ($filters as $name => $value) {
            $customFilter .= "`$name` = '$value', ";
        }
        
        $query = 'SELECT id_ba_group, ba_group_name FROM `mod_bam_ba_groups` WHERE ' .
            rtrim($customFilter, ', ') . ' ' .
            $aclFilter . ' ' .
            'ORDER BY ba_group_name';
        $DBRES2 = $this->_db->query($query);
        while ($rowb = $DBRES2->fetchRow()) {
            $baGroupList[] = array('ba_group_name' => $rowb['ba_group_name'], 'id_ba_group' => $rowb['id_ba_group']);
        }
        
        return $baGroupList;
    }

    /**
     * Sets visibility of a BA group
     * @param int $idBaGroup
     * @param int $visibility
     * @param array $multiSelect
     */
	public function setVisibility($idBaGroup, $visibility, $multiSelect = null)
	{
		$list = "";
		if (isset($multiSelect)) {
			foreach ($multiSelect as $key => $value) {
				if ($list != "") {
					$list .= ", ";
				}
				$list .= "'".$key."'";
			}
			if ($list == "") {
				$list = "''";
			}
		} else {
			$list = "'".$idBaGroup."'";
		}
		$query = "UPDATE `mod_bam_ba_groups` " .
				"SET visible = '".$visibility."' " .
				"WHERE id_ba_group IN ($list)";
		$this->_db->query($query);
	}

	/**
     * Updates ACL
     * @param int $ba_g_id
     * @param array $conf
     * @return null
     */
	private function updateBA_ACL($ba_g_id = null, $conf = null)
	{
		if (!isset($ba_g_id)) {
			return null;
		}
		$rq = "DELETE FROM `mod_bam_acl` ";
		$rq .= "WHERE ba_group_id = '".$ba_g_id."'";
		$resULT = $this->_db->query($rq);

		$tab = array();
		if (isset($this->_form)) {
			$tab = $this->_form->getSubmitValue("bam_acl");
		} elseif (isset($conf)) {
			$tab = $conf['bam_acl'];
		}
		for ($i = 0; $i < count($tab); $i++)	{
			$rq = "INSERT INTO `mod_bam_acl` ";
			$rq .= "(acl_group_id, ba_group_id) ";
			$rq .= "VALUES ";
			$rq .= "('".$tab[$i]."', '".$ba_g_id."')";
			$resULT = $this->_db->query($rq);
		}
        $this->updateBvAclResource($ba_g_id, $tab);
	}
    
    /**
     * 
     * @param type $idBv
     * @param type $tabBaAclGroup
     */
    private function updateBvAclResource($idBv, $tabBaAclGroup)
    {
        $aclResourceId = $this->getBvAclResourceId($idBv);
        
        $rq = "DELETE FROM `acl_res_group_relations` ";
		$rq .= "WHERE acl_res_id = '" . $aclResourceId . "'";
		$this->_db->query($rq);
        
        // Link Acl Resource to ACL Group
        foreach ($tabBaAclGroup as $baAclGroup) {
            $rq = "INSERT INTO `acl_res_group_relations` ";
			$rq .= "(acl_group_id, acl_res_id) ";
			$rq .= "VALUES ";
			$rq .= "('" . $baAclGroup . "', '" . $aclResourceId . "')";
			$this->_db->query($rq);
        }
    }
    
    /**
     * 
     * @param int $idBv
     * @return int
     */
    private function getBvAclResourceId($idBv)
    {
        $bvAclResourceName = 'HiddenBv_' . $idBv . '_DoNotDelete';
        
        // Get Acl Resource Id
        $queryGetAclResourceId = "SELECT `acl_res_id` "
            . "FROM `acl_resources` "
            . "WHERE `acl_res_name` = '$bvAclResourceName'";
        $resAclResource = $this->_db->query($queryGetAclResourceId);
        $rowAclResource = $resAclResource->fetchRow();
        return $rowAclResource['acl_res_id'];
    }

	/**
     * Updates relation of Group <-> BA
     * @param int $ba_g_id
     * @param array $conf
     * @return null
     */
	private function updateBA_List($ba_g_id = null, $conf = null, $aclResourceId = null)
	{
		if (!isset($ba_g_id)) {
			return null;
		}

		$rq = "DELETE FROM `mod_bam_bagroup_ba_relation` ";
		$rq .= "WHERE id_ba_group = '".$ba_g_id."'";
		$resULT = $this->_db->query($rq);

		$tab = array();
		if (isset($this->_form)) {
			$tab = $this->_form->getSubmitValue("ba_list");
		} elseif (isset($conf)) {
			$tab = $conf['ba_list'];
		}
		for($i = 0; $i < count($tab); $i++)	{
			$rq = "INSERT INTO `mod_bam_bagroup_ba_relation` ";
			$rq .= "(id_ba, id_ba_group) ";
			$rq .= "VALUES ";
			$rq .= "('".$tab[$i]."', '".$ba_g_id."')";
			$resULT = $this->_db->query($rq);
		}
        
        notifyAclConfChangedByBv($this->_db, $ba_g_id);
	}

	/**
     * Deletes a BA group from database
     * @param int $idBaGroup
     * @param array $multiSelect
     */
	public function deleteBAGroup($idBaGroup, $multiSelect = null)
	{
		$list = "";
		if (isset($multiSelect)) {
			foreach ($multiSelect as $key => $value) {
				if ($list != "") {
					$list .= ", ";
				}
				$list .= "'".$key."'";
			}
			if ($list == "") {
				$list = "''";
			}
		} else {
			$list = "'".$idBaGroup."'";
		}
		$query = "DELETE FROM `mod_bam_ba_groups` WHERE id_ba_group IN ($list)";
		$this->_db->query($query);
        $this->deleteLinkedAclResources($idBaGroup, $multiSelect);
	}
    
    /**
     * 
     * Deletes a BA group from database
     * @param int $idBaGroup
     * @param array $multiSelect
     */
    public function deleteLinkedAclResources($idBaGroup, $multiSelect = null)
    {
        $list = "";
        if (isset($multiSelect)) {
			foreach ($multiSelect as $key => $value) {
				if ($list != "") {
					$list .= ", ";
				}
				$list .= "'". $this->getBvAclResourceId($key)."'";
			}
			if ($list == "") {
				$list = "''";
			}
		} else {
			$list = "'".$this->getBvAclResourceId($idBaGroup)."'";
		}
        
        $queryDeleteBvAclResource = "DELETE FROM acl_resources WHERE acl_res_id IN ($list)";
        $this->_db->query($queryDeleteBvAclResource);
    }

    /**
     * Returns name of a BA view
     * @param int $idBaGroup
     * @return mixed
     */
	public function getBaViewName($idBaGroup)
	{
		$query2 = "SELECT ba_group_name FROM `mod_bam_ba_groups` WHERE id_ba_group = '" . $idBaGroup ."' LIMIT 1";
		$res2 = $this->_db->query($query2);
		if (!$res2->numRows()) {
			return null;
		}
		$row = $res2->fetchRow();
		return $row['ba_group_name'];
	}

	/**
     * returns conf of ba group
     * @param int $idBaGroup
     * @return mixed
     */
	public function getBaViewConf($idBaGroup)
	{
		$tab = array();
		$query2 = "SELECT * FROM `mod_bam_ba_groups` WHERE id_ba_group = '" . $idBaGroup."' LIMIT 1";
		$res2 = $this->_db->query($query2);
		if (!$res2->numRows()) {
			return null;
		}
		$row = $res2->fetchRow();
		foreach ($row as $key => $value) {
			$tab[$key] = $value;
		}

		$query = "SELECT * FROM `mod_bam_bagroup_ba_relation` WHERE id_ba_group = '". $idBaGroup ."'";
		$res = $this->_db->query($query);
		while ($row = $res->fetchRow()) {
			$tab['ba_list'][] = $row['id_ba'];
		}

		$query = "SELECT * FROM `mod_bam_acl` WHERE ba_group_id = '". $idBaGroup ."'";
		$res = $this->_db->query($query);
		while ($row = $res->fetchRow()) {
			$tab['bam_acl'][] = $row['acl_group_id'];
		}
		return $tab;
	}

	/**
     * Duplicate a ba group
     * @param array $multiSelect
     * @return null
     */
	public function duplicate($multiSelect = null)
	{
		if (!isset($multiSelect)) {
			return null;
		}
		foreach ($multiSelect as $key => $value) {
			$ba_group_conf = array();
			$ba_group_conf = $this->getBaViewConf($key);
			if (isset($ba_group_conf)) {
				$nb_dup = 0;
				if (isset($_POST['dup_'.$key]) && is_numeric($_POST['dup_'.$key])) {
					$nb_dup = $_POST['dup_'.$key];
					for ($i = 0; $i < $nb_dup; $i++) {
						$this->insertBAGroupInDB($ba_group_conf, $i + 1);
					}
				}
			}
		}
	}
}