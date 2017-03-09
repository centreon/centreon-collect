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
require_once _CENTREON_PATH_ . 'www/modules/centreon-bam-server/core/class/CentreonBam/Acl.php';

class CentreonbamService
{
    private $db;
    private $dbMon;
    private $baObj;
    private $bamAclObj;

    public function __construct()
    {
        global $centreon;
        $userId = $centreon->user->user_id;

        $this->db = new CentreonDB();
        $this->dbMon = new CentreonDB('centstorage');
        $this->baObj = new CentreonBam_Ba($this->db, null, $this->dbMon);
        $this->bamAclObj = new CentreonBam_Acl($this->db, $userId);
    }

    public function getVirtualServiceIds()
    {
        $hostId = $this->baObj->getVirtualHostId();

        $bas = $this->bamAclObj->getBa();
        $serviceIds = array();
        foreach ($bas as $baId => $value) {
            $serviceIds[] = $this->baObj->getCentreonServiceBaId($baId, $hostId);
        }

        return array(
            'BAM' => $serviceIds
        );
    }

    public function getMonitoringFullName($serviceId)
    {
        if (!is_numeric($serviceId)) {
            return null;
        }

        $name = null;

        $query = 'SELECT h.name, s.description, s.display_name '
            . 'FROM hosts h, services s '
            . 'WHERE h.host_id = s.host_id '
            . 'AND s.enabled = "1" '
            . 'AND s.service_id = ' . $serviceId;
        $result = $this->dbMon->query($query);
        while ($row = $result->fetchRow()) {
            if (preg_match('/^ba_\d+$/', $row['description']) &&  preg_match('/^_Module_BAM\S*$/', $row['name'])) {
                $name = 'BAM - ' . $row['display_name'];
            }
        }

        return $name;
    }
}
