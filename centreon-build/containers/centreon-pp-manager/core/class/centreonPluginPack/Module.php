<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2016 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

require_once 'iModule.php';

abstract class CentreonPluginPack_Module implements CentreonPluginPack_iModule
{
    protected $db;

    protected $installed_plugin_packs = null;

    /**
     * Constructor
     *
     * @param CentreonDB $database The database connection
     */
    public function __construct($database)
    {
        $this->db = $database;
    }

    /**
     * Get the list of installed pluginpack
     *
     * array(
     *   'slugvalue' => array(
     *     'slug' => 'slugvalue',
     *     'version' => 'versionvalue'
     *   )
     * )
     *
     * @return array The list of installed pluginpack
     */
    protected function getListInstalled()
    {
        if (!is_null($this->installed_plugin_packs)) {
            return $this->installed_plugin_packs;
        }

        $query = 'SELECT `slug`, `name`, `version`, `complete` FROM `mod_ppm_pluginpack` ORDER BY `name`';
        $res = $this->db->query($query);
        if (PEAR::isError($res)) {
            throw new Exception('Error during get the list of installed pluginpack');
        }
        $list = array();
        while ($row = $res->fetchRow()) {
            $list[$row['slug']] = $row;
            $list[$row['slug']]['complete'] = ($list[$row['slug']]['complete'] == '0') ? false : true;
        }

        $this->installed_plugin_packs = $list;

        return $this->installed_plugin_packs;
    }

    /**
     * Check if a plugin pack is installed
     *
     * @return array The plugin pack not installed
     */
    protected function isInstalled(&$pluginPack)
    {
        $installed = $this->getListInstalled();

        if (isset($installed[$pluginPack['slug']])) {
            $pluginPack['installed'] = true;
            return true;
        } else {
            return false;
        }
    }
}
