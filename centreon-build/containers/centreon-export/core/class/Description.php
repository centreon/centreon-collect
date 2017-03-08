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

namespace CentreonExport;

use \CentreonExport\DBManager;

class Description
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
     * This method return the list of descriptions linked with plugin
     * 
     * @param int $iId Id of plugin
     * @return array
     */
    public function getDescriptionByPlugin($iId)
    {
        $aDatas = array();
        if (empty($iId)) {
            return $aDatas;
        }
        $sQuery = "SELECT d.description_id, d.description_text, l.locale_short_name, l.locale_long_name, l.locale_img "
            . "FROM mod_export_description d, locale l "
            . "WHERE d.description_plugin_id = :plugin_id "
            . "AND d.description_locale = l.locale_id";
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetchAll(\PDO::FETCH_ASSOC|\PDO::FETCH_UNIQUE);
        $descriptions = array();
        foreach ($aData as $description) {
            $descriptions[$description['locale_short_name']] = $description['description_text'];
        }
        return $descriptions;
    }
}
