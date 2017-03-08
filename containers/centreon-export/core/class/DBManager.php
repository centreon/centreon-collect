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

require_once _CENTREON_PATH_."/config/centreon.config.php";

class DBManager
{
    /**
     *
     * @var type 
     */
    public $db = null;
    
    /**
     * 
     * @return type
     */
    public function __construct()
    {
        if (is_null($this->db)) {
            try {
                $this->db = new \PDO("mysql:dbname=pdo;host=" . hostCentreon . ";dbname=" . db, user, password, array(\PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8"));
                $this->db->setAttribute(\PDO::ATTR_ERRMODE, \PDO::ERRMODE_EXCEPTION);
            } catch (\PDOException $e) {
                echo 'Connexion échouée : ' . $e->getMessage();
            }
        }
        return $this->db;
    }

    /**
     * 
     * @param boolean $mode
     */
    public function autocommit($mode = false)
    {
        $this->db->autoCommit($mode);
        $this->debug = 0;
    }
    
    /**
     * 
     */
    public function commit()
    {
        $this->db->commit();
    }
    
    /**
     * 
     */
    public function rollback()
    {
        $this->db->rollback();
    }
}
