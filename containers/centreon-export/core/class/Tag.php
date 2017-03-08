<?php
/*
 * Copyright 2005-2015 Centreon
 * Centreon is developped by : Julien Mathis and Romain Le Merlus under
 * GPL Licence 2.0.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation ; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses>.
 *
 * Linking this program statically or dynamically with other modules is making a
 * combined work based on this program. Thus, the terms and conditions of the GNU
 * General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this program give Centreon
 * permission to link this program with independent modules to produce an executable,
 * regardless of the license terms of these independent modules, and to copy and
 * distribute the resulting executable under terms of Centreon choice, provided that
 * Centreon also meet, for each linked independent module, the terms  and conditions
 * of the license of that module. An independent module is a module which is not
 * derived from this program. If you modify this program, you may extend this
 * exception to your version of the program, but you are not obliged to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 *
 * For more information : contact@centreon.com
 *
 */

namespace CentreonExport;

use \CentreonExport\DBManager;

class Tag
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
     * This method return list of tag linked with the plugin
     * 
     * @param int $iId identifiant of plugin
     * @return array
     */
    public function getTagsByPlugin($iId)
    {
        $aDatas = array();
        if (empty($iId)) {
            return $aDatas;
        }
        $sQuery = "SELECT tags_id, tags_name FROM `mod_export_tags` JOIN `mod_export_tags_relation` ON tag_id = tags_id WHERE plugin_id = :plugin_id ";
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetchAll(\PDO::FETCH_ASSOC|\PDO::FETCH_UNIQUE);
        $tags = array();
        
        foreach ($aData as $key => $valeur) {
            $tags[] = array('id' => $key, 'text' => $valeur['tags_name']);
        }

        return $tags;
    }

    /**
     * This method delete all link between tags and the plugin
     * 
     * @param int $iIdPlugin identifiant of plugin
     */
    private function deleteTagByPlugin($iIdPlugin)
    {
        if (empty($iIdPlugin)) {
            return false;
        }
        $sQuery = 'DELETE FROM `mod_export_tags_relation` WHERE plugin_id = :plugin_id ';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
        try {
            $sth->execute();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }
    }

    /**
     * This method add tags and link between tags and the plugin
     * 
     * @param int $iIdPlugin identifiant of plugin
     * @param array $aTags List of tags
     * @return boolean
     */
    public function addTagsInPlugin($iIdPlugin, $aTags)
    {
        if (empty($iIdPlugin) || count($aTags) == 0) {
            return false;
        }

        $this->deleteTagByPlugin($iIdPlugin);
        $sQuery = 'INSERT INTO `mod_export_tags_relation` (plugin_id, tag_id) VALUES (:plugin_id , :tag_id)';
        $sth = $this->db->db->prepare($sQuery);

        foreach ($aTags as $key => $value) {
            if (is_numeric($value)) {
                $iId = $value;
            } else {
                $iId = $this->tagExist($value);
                if (!$iId || $iId == 0) {
                    $iId = $this->insertTag($value);
                }
            }

            $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
            $sth->bindParam(':tag_id', $iId, \PDO::PARAM_INT);
            try {
                $sth->execute();
            } catch (\PDOException $e) {
                echo "Error " . $e->getMessage();
            }
        }
    }

    /**
     * This method insert tag in the BDD
     * 
     * @param string $sLib Name of tag
     * @return boolean
     */
    private function insertTag($sLib)
    {
        if (empty($sLib)) {
            return false;
        }
        $sQuery = 'INSERT INTO `mod_export_tags` (tags_name) VALUES (:tags_name)';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':tags_name', $sLib, \PDO::PARAM_STR);

        try {
            $sth->execute();
            $id = $this->db->db->lastInsertId();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }

        return $id;
    }

    /**
     * This method check if tag exist in BDD
     * 
     * @param string $sLib
     * @return boolean
     */
    private function tagExist($sLib)
    {
        if (empty($sLib)) {
            return false;
        }
        $id = 0;

        $sQuery = 'SELECT tags_id FROM `mod_export_tags`  WHERE tags_name = :tags_name';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':tags_name', $sLib, \PDO::PARAM_STR);

        try {
            $sth->execute();
            $id = $sth->rowCount();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }

        return $id;
    }
}
