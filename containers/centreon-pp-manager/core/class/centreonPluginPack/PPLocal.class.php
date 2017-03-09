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

namespace CentreonPluginPackManager;

class PPLocal
{
    protected $db;
    protected $slug;

    /**
     * Constructor
     *
     * @param $slug     The slug used to uniquely recognize a PP.
     * @param $db       The database object.
     */
    public function __construct($slug, $db)
    {
        $this->slug = $slug;
        $this->db = $db;
    }

    /**
     * Get all properties of a PP.
     *
     * @return An associative array of the PP properties.
     */
    public function getProperties()
    {
        $pluginPack = array();
        $query = "SELECT * FROM mod_ppm_pluginpack WHERE slug = '" . $this->slug . "'";
        $res = $this->db->query($query);
        if ($res->numRows()) {
            $data = $res->fetchRow();
            $pluginPack = $data;
        }
        return $pluginPack;
    }

    /**
     * Check if this PP is actually installed.
     *
     * @return bool  True or false
     */
    public function isInstalled()
    {
        $query = "SELECT pluginpack_id FROM mod_ppm_pluginpack WHERE slug='" . $this->slug . "'";
        $res = $this->db->query($query);
        if ($res->numRows()) {
            $row = $res->fetchRow();
            $retval = $row['pluginpack_id'];
        } else {
            $retval = false;
        }
        return $retval;
    }
}
