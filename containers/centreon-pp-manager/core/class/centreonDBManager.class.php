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

require_once $centreon_path . "/www/class/centreonDB.class.php";

class CentreonDBManager extends CentreonDB {
    
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