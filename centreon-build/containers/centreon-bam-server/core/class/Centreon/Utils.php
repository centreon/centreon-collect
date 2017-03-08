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

class CentreonBam_Utils
{
    protected $db;

    /**
     * Constructor
     *
     * @param CentreonBam_Db
     * @return void
     */
    public function __construct($db)
    {
        $this->db = $db;
    }

    /**
     * Get Db Layer
     *
     * @return string
     */
    public function getDbLayer()
    {
        static $dbLayer = null;

        if (!isset($dbLayer)) {
            $dbLayer = "ndo";
            $query = "SELECT `value` FROM `options` WHERE `key` = 'broker' LIMIT 1";
            $res = $this->db->query($query);
            if ($res->numRows()) {
                $row = $res->fetchRow();
                $dbLayer = $row['value'];
            }
        }
        return $dbLayer;
    }

    /**
     * Checks if table exists
     *
     * @param CentreonBam_Db $db
     * @param string $tableName
     * @return bool
     */
    public function tableExists($db, $tableName)
    {
        static $tables = array();
        static $checked = array();

        if (!isset($checked[$tableName])) {
            $query = "SHOW TABLES LIKE '".$tableName."'";
            $res = $db->query($query);
            $checked[$tableName] = 1;
            if ($res->numRows()) {
                $tables[$tableName] = 1;
                return true;
            } else {
                $tables[$tableName] = 0;
                return false;
            }
        } elseif (isset($tables[$tableName]) && $tables[$tableName] == 1) {
            return true;
        }
        return false;
    }

    /**
     * Format data source name
     *
     * @param CentreonDB $db
     * @param string $rawDsName
     * @return string
     */
    public static function formatDsName($db, $rawDsName)
    {
        $res = $db->query("SELECT `value` FROM informations WHERE `key` = 'version'");
        $row = $res->fetchRow();
        if (!isset($row['value'])) {
            throw new Exception('Could not find version');
        }
        if (version_compare($row['value'], '2.5.0', '<')) {
            return substr($rawDsName,0 , 19);
        }
        return "value";
    }
}
