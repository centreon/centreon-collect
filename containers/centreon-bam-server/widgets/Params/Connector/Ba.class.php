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

require_once "class/centreonWidget/Params/List.class.php";
require_once ("./modules/centreon-bam-server/core/common/header.php");

/**
 * Connector for get the list of BA for widget parameters
 */
class CentreonWidgetParamsConnectorBa extends CentreonWidgetParamsList
{
    public function __construct($db, $quickform, $userId)
    {
        parent::__construct($db, $quickform, $userId);
    }
    
    public function getListValues($paramId)
    {
        global $oreon;
        static $tab;

        if (!isset($tab)) {
            $dbNdo = new CentreonDB("centstorage");
            $ba_meth = new CentreonBam_Ba($this->db, null, $dbNdo);
            $ba_acl_meth = new CentreonBam_Acl($this->db, $this->userId);
            
            if (!$oreon->user->admin) {
                $items2 = $ba_acl_meth->getBa();
                foreach ($items2 as $key => $value) {
                    $items[$key] = $ba_meth->getBA_Name($key);
                }
            } else {
                $items = $ba_meth->getBA_list();
            }
            $tab[null] = null;
            foreach ($items as $key => $value) {
                $tab[$key] = $value;
            }
        }
        return $tab;
    }
}