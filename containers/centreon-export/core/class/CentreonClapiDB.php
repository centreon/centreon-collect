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

namespace CentreonExport;

require_once realpath(dirname(__FILE__) . "/../../../../class/centreonDB.class.php");

class CentreonClapiDB extends \CentreonDB {
    
    public function autocommit($mode = false) {
        $this->db->autoCommit($mode);
        # Need raise exception
        $this->debug = 0;
    }
    
    public function commit() {
        $this->db->commit();
    }
    
    public function rollback() {
        $this->db->rollback();
    }
}

?>