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

require_once _CENTREON_PATH_ . 'www/class/centreonDB.class.php';
require_once _CENTREON_PATH_ . 'www/modules/centreon-bam-server/core/class/CentreonBam/Ba.php';

class CentreonbamHost
{
    private $db;
    private $dbMon;
    private $baObj;

    public function __construct()
    {
        global $centreon;
        $userId = $centreon->user->user_id;

        $this->db = new CentreonDB();
        $this->dbMon = new CentreonDB('centstorage');
        $this->baObj = new CentreonBam_Ba($this->db, null, $this->dbMon);
    }

    public function getVirtualHosts()
    {
        $hostId = $this->baObj->getVirtualHostId();

        return array(
            $hostId => 'BAM'
        );
    }

}
