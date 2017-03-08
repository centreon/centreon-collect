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
class CentreonWidgetParamsConnectorBv extends CentreonWidgetParamsList
{
    public function __construct($db, $quickform, $userId)
    {
        parent::__construct($db, $quickform, $userId);
    }
    
    public function getListValues($paramId)
    {
        static $tab;

        if (!isset($tab)) {
            $ba_acl = new CentreonBam_Acl($this->db, $this->userId);
            $query = "SELECT id_ba_group, ba_group_name FROM `mod_bam_ba_groups` WHERE visible = '1' " . $ba_acl->queryBuilder("AND", "id_ba_group", $ba_acl->getBaGroupStr()) . " ORDER BY ba_group_name";
            $res = $this->db->query($query);
            $tab[null] = null;
            if (false === PEAR::isError($res)) {
                while ($row = $res->fetchRow()) {
                    $tab[$row['id_ba_group']] = $row['ba_group_name'];
                }
            }
        }
        return $tab;
    }
}