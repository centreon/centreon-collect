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
class CentreonBam_Icon
{
	protected $_db;
	protected $_cacheIcon;	

	/*
	 *  Constructor
	 */
	function __construct($db)
	{
		$this->_db = $db;
		$this->_cacheIcon = array();
		$this->_createCache();
	}
	
	/*
	 *  Creates cache
	 */
	protected function _createCache()
	{
	    $rq = "SELECT img.img_id, img.img_path as file, dir.dir_alias as folder 
	    	   FROM view_img img, view_img_dir dir, view_img_dir_relation rel 
	    	   WHERE img.img_id = rel.img_img_id 
	    	   AND rel.dir_dir_parent_id = dir.dir_id ";
	    $res = $this->_db->query($rq);
	    while ($row = $res->fetchRow()) {
	        $this->_cacheIcon[$row['img_id']] = './img/media/' . $row['folder'] . '/' . $row['file'];
	    }
	}
	
	/*
	 *  Returns icon path
	 */
	public function getFullIconPath($iconId)
	{
	    if (isset($this->_cacheIcon[$iconId])) {
	        return $this->_cacheIcon[$iconId];
	    }
	    return null;
	}
}
?>