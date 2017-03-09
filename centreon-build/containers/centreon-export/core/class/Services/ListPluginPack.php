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
use \CentreonExport\Command;
use \CentreonExport\HostTemplate;
use \CentreonExport\ServiceTemplate;

class ListPluginPack
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
     * This method returns the list of plugins pack
     * 
     * @param array $aParams
     * @param int $count
     * @return array
     */
    public function getList($aParams = array(), $count = 0)
    {
        if (isset($aParams['num'])) {
            $num = $aParams['num'];
        } else {
            $num = 0;
        }
        if (isset($aParams['limit'])) {
            $limit = $aParams['limit'];
        } else {
            $limit = 50;
        }

        $aReturn = array();

        $sQuery = "SELECT *, "
            . " (CASE WHEN plugin_last_update = '' or plugin_last_update is null THEN '' ELSE FROM_UNIXTIME(plugin_last_update) END) as last_update, "
            . " (CASE WHEN plugin_time_generate = '' or plugin_time_generate is null THEN '' ELSE FROM_UNIXTIME(plugin_time_generate) END) as time_generate "
            . " FROM `mod_export_pluginpack` "
            . " WHERE  1 = 1 ";
        if (!empty($aParams['search'])) {
            $sQuery .= " AND `plugin_name` like " . $this->db->db->quote("%" . $aParams['search'] . "%", \PDO::PARAM_STR);
        }
        if (isset($aParams['version']) && $aParams['version'] != '') {
            $sQuery .= " AND `plugin_version` like " . $this->db->db->quote("%" . $aParams['version'] . "%", \PDO::PARAM_STR);
        }
        if (!empty($aParams['modified']) && in_array($aParams['modified'], array(0, 1))) {
            if ($aParams['modified'] == 0) {
                $sQuery .= " AND `plugin_time_generate` = `plugin_last_update` ";
            } elseif ($aParams['modified'] == 0) {
                $sQuery .= " AND `plugin_time_generate` <> `plugin_last_update` ";
            }
        }
        if (!empty($aParams['dateLimit'])) {
            $sOperaror = ' >= ';
            if (!empty($aParams['operator'])) {
                $sOperaror = $aParams['operator'];
            }
            $sQuery .= " AND `plugin_time_generate`  " . $sOperaror . " " . $this->db->db->quote($aParams['dateLimit'], \PDO::PARAM_STR);
        }
        if ($count == 0) {
            $sQuery .= " ORDER BY `plugin_name` ASC LIMIT " . $num * $limit . ", " . $limit;

            $oHost = new HostTemplate();
            $oService = new ServiceTemplate();
            $oCommand = new Command();
        }
        $res = $this->db->db->query($sQuery);


        if ($count == 0) {
            while ($data = $res->fetch(\PDO::FETCH_ASSOC)) {
                $data['nbHostTpl'] = $oHost->getNbHostTemplateByPlugin($data['plugin_id']);
                $data['nbServiceTpl'] = $oService->getNbServiceTemplateByPlugin($data['plugin_id']);
                $data['nbCommand'] = $oCommand->getNbCommandByPlugin($data['plugin_id']);
                $aReturn[$data['plugin_id']] = $data;
            }
        } else {
            $aReturn[] = $res->rowCount();
        }
        return $aReturn;
    }

    /**
     * This method is used for migration from the old to the new plugins package management system
     */
    public function migrate()
    {
        $sth = $this->db->db->prepare('INSERT INTO `mod_export_pluginpack` (`plugin_name`, `plugin_display_name`, `plugin_slug`, `plugin_author`, `plugin_path`,  `plugin_status`, `plugin_version`, `plugin_last_update`) VALUES (:name, :display_name, :slug, :author, :path, :status, :version, :update)');

        $tabDirectory = array();
        $dir = "/srv/plugins-packs-repositories/";
        if (is_dir($dir)) {
            if ($dh = opendir($dir)) {
                if (count($elem) > 0) {
                    $this->db->db->query(" DELETE FROM `mod_export_pluginpack`");
                }
                while ($elem = readdir($dh)) {
                    if (is_dir($dir . $elem) && $elem != "." && $elem != ".." && $elem != "Centreon") {
                        $status = 'KO';
                        $update = '';
                        if (file_exists($dir . $elem . "/templates/plugins-pack.xml")) {
                            $tab = stat($dir . $elem . "/templates/plugins-pack.xml");
                            $status = 'OK';
                            $update = $tab[9];
                        }
                        $path = $dir . $elem;
                        $author = 'Centreon';
                        $version = '1';

                        $sth->bindParam(':name', $elem, \PDO::PARAM_STR);
                        $sDiplayName = substr($elem, 0, 15);
                        $sth->bindParam(':display_name', $sDiplayName, \PDO::PARAM_STR);
                        $sth->bindParam(':slug', $elem, \PDO::PARAM_STR);
                        $sth->bindParam(':author', $author, \PDO::PARAM_STR);
                        $sth->bindParam(':path', $path, \PDO::PARAM_STR);
                        $sth->bindParam(':status', $status, \PDO::PARAM_STR);
                        $sth->bindParam(':version', $version, \PDO::PARAM_STR);
                        $sth->bindParam(':update', $update, \PDO::PARAM_STR);
                        $sth->execute();
                    }
                }
            }
        }
    }

    /**
     * This method returns the list of plugins except the one already used
     * 
     * @param string $sSlug
     * @return array
     */
    public function getAll($sSlug)
    {
        $sQuery = "SELECT SQL_CALC_FOUND_ROWS DISTINCT `plugin_slug`, `plugin_name`  FROM `mod_export_pluginpack` WHERE `plugin_slug` <> :slug ORDER BY `plugin_name` ";
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':slug', $sSlug, \PDO::PARAM_STR);
        $sth->execute();

        $pluginList = array();
        while ($data = $sth->fetch()) {
            $pluginList[$data['plugin_slug']] = $data['plugin_name'];
        }

        return $pluginList;
    }

    /**
     * This method returns the list of versions of plugin
     * 
     * @param styring $sSlug
     * @return array
     */
    public function getVersion($sSlug)
    {
        if (empty($sSlug)) {
            return array();
        }
        $sQuery = "SELECT DISTINCT `plugin_version`  FROM `mod_export_pluginpack` "
            . " WHERE `plugin_slug` = :slug ORDER BY `plugin_version` ";

        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':slug', $sSlug, \PDO::PARAM_STR);
        $sth->execute();

        $versionList = array();
        while ($data = $sth->fetch()) {
            $versionList[] = $data['plugin_version'];
        }

        return $versionList;
    }
}
