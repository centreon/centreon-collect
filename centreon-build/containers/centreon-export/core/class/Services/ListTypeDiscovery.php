<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2015 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

namespace CentreonExport\Services;

use \CentreonExport\DBManager;

class ListTypeDiscovery
{
    /**
     *
     * @var type 
     */
    public $db;

    /**
     * 
     */
    public function __construct()
    {
        $this->db = new DBManager();
    }
 
    /**
     * This method returns the list of Type of discovery
     * 
     * @param array $aParams
     * @return array
     */
    public function getList($aParams = array())
    {
        $res = $this->db->db->query("SELECT slug, name 
                            FROM mod_export_discovery_category ORDER BY name");
        $list = array('' => '');
        while ($row = $res->fetch(\PDO::FETCH_ASSOC)) {
            $list[$row['slug']] = $row['name'];
        }
        return $list;
    }
}
