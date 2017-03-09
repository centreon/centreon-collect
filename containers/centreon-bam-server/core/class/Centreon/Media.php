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
 *  Class used for managing images
 */
class CentreonBam_Media
{
	protected $_db;
    protected $_filenames;
    protected $_serviceImageId;
    protected $_hostImageId;

	/*
	 *  Constructor
	 */
	function __construct($db)
	{
		$this->_db = $db;
		$this->_filenames = array();
		$this->_serviceImageId = array();
		$this->_hostImageId = array();
	}

	/*
	 *  Returns ID of target directory
	 */
	function getDirectoryId($dirname)
	{
		$query = "SELECT dir_id FROM view_img_dir WHERE dir_name = '".$dirname."' LIMIT 1";
		$RES = $this->_db->query($query);
		$dir_id = null;
		if ($RES->numRows()) {
			$row =& $RES->fetchRow();
			$dir_id = $row['dir_id'];
		}
		return $dir_id;
	}

	/*
	 *  Returns ID of target Image
	 */
	function getImageId($imagename, $dirname = null)
	{
		if (!isset($dirname)) {
			$tab = explode("/", $imagename);
			isset($tab[0]) ? $dirname = $tab[0] : $dirname = null;
			isset($tab[1]) ? $imagename = $tab[1] : $imagename = null;
		}

		if (!isset($imagename) || !isset($dirname)) {
			return null;
		}

		$query = "SELECT img.img_id ".
				"FROM view_img_dir dir, view_img_dir_relation rel, view_img img ".
				"WHERE dir.dir_id = rel.dir_dir_parent_id " .
				"AND rel.img_img_id = img.img_id ".
				"AND img.img_path = '".$imagename."' ".
				"AND dir.dir_name = '".$dirname."' " .
				"LIMIT 1";
		$RES = $this->_db->query($query);
		$img_id = null;
		if ($RES->numRows()) {
			$row = $RES->fetchRow();
			$img_id = $row['img_id'];
		}
		return $img_id;
	}

	/**
	 * Returns the filename from a given id
	 *
	 * @param int $imgId
	 * @return string
	 */
	public function getFilename($imgId = null)
	{
	    if (!isset($imgId)) {
	        return "";
	    }
	    if (count($this->_filenames)) {
            if (isset($this->_filenames[$imgId])) {
	            return $this->_filenames[$imgId];
            } else {
                return "";
            }
	    }
	    $query = "SELECT img_id, img_path, dir_alias
	    		  FROM view_img vi, view_img_dir vid, view_img_dir_relation vidr
	    		  WHERE vidr.img_img_id = vi.img_id
	    		  AND vid.dir_id = vidr.dir_dir_parent_id";
	    $res = $this->_db->query($query);
	    $this->_filenames[0] = 0;
	    while ($row = $res->fetchRow()) {
            $this->_filenames[$row['img_id']] = $row["dir_alias"]."/".$row["img_path"];
	    }
	    if (isset($this->_filenames[$imgId])) {
            return $this->_filenames[$imgId];
	    }
	    return "";
	}

	/**
	 * Returns the image id of a service
	 *
	 * @param int $serviceId
	 * @return int
	 */
	public function getServiceImageId($serviceId = null)
	{
        if (!isset($serviceId)) {
            return "";
        }
        if (count($this->_serviceImageId)) {
            if (isset($this->_serviceImageId[$serviceId])) {
                return $this->_serviceImageId[$serviceId];
            } else {
                return "";
            }
        }
        $query = "SELECT esi_icon_image, service_service_id
	    		  FROM extended_service_information
	    		  WHERE esi_icon_image IS NOT NULL";
        $res = $this->_db->query($query);
	    $this->_serviceImageId[0] = 0;
	    while ($row = $res->fetchRow()) {
            $this->_serviceImageId[$row['service_service_id']] = $row["esi_icon_image"];
	    }
	    if (isset($this->_serviceImageId[$serviceId])) {
            return $this->_serviceImageId[$serviceId];
	    }
	    return "";
	}

	/**
	 * Returns the image id of a host
	 *
	 * @param int $hostId
	 * @return int
	 */
	public function getHostImageId($hostId = null)
	{
        if (!isset($hostId)) {
            return "";
        }
        if (count($this->_hostImageId)) {
            if (isset($this->_hostImageId[$hostId])) {
                return $this->_hostImageId[$hostId];
            } else {
                return "";
            }
        }
        $query = "SELECT ehi_icon_image, host_host_id
	    		  FROM extended_host_information
	    		  WHERE ehi_icon_image IS NOT NULL";
	    $res = $this->_db->query($query);
	    $this->_hostImageId[0] = 0;
	    while ($row = $res->fetchRow()) {
            $this->_hostImageId[$row['host_host_id']] = $row["ehi_icon_image"];
	    }
	    if (isset($this->_hostImageId[$hostId])) {
            return $this->_hostImageId[$hostId];
	    }
	    return "";
	}
}
