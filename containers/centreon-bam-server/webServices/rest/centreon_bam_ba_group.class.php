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

global $centreon_path;
require_once $centreon_path . "/www/class/centreonBroker.class.php";

if (file_exists($centreon_path . "/www/include/common/webServices/rest/webService.class.php")) {
    // Centreon < 2.8
    require_once $centreon_path . "/www/include/common/webServices/rest/webService.class.php";
} else {
    // Centreon >= 2.8
    require_once $centreon_path . "/www/api/class/webService.class.php";
}

require_once $centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/Acl.php";
require_once $centreon_path . "www/modules/centreon-bam-server/core/class/Centreon/Utils.php";


class CentreonBamBaGroup  extends CentreonWebService
{
    /**
     *
     * @var type 
     */
    protected $pearDB;
    
    /**
     *
     * @var type 
     */
    protected $pearDBMonitoring;

    /**
     * Get default report values
     * 
     * @param array $args
     */
    public function getList()
    {
        global $centreon;
        $ba_acl = new CentreonBam_Acl($this->pearDB, $centreon->user->user_id);

        if (false === isset($this->arguments['q'])) {
            $q = '';
        } else {
            $q = $this->arguments['q'];
        }
        
        $query = "SELECT SQL_CALC_FOUND_ROWS id_ba_group, ba_group_name "
            . "FROM `mod_bam_ba_groups` "
            . "WHERE ba_group_name LIKE '%" . $this->pearDB->escape($q) . "%' ".
            $ba_acl->queryBuilder("AND", "id_ba_group", $ba_acl->getBaStr());

        $DBRESULT = $this->pearDB->query($query);
        
        $total = $this->pearDB->numberRows();
        
        $bas = array();
        while ($row = $DBRESULT->fetchRow()) {
            $bas[] = array(
                'id' => $row['id_ba_group'],
                'text' => $row['ba_group_name']
            );
        }
        return array(
            'items' => $bas,
            'total' => $total
        );
    }
}