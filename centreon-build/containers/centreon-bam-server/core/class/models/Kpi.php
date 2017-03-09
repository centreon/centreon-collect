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

require_once "Centreon/Object/Object.php";

/**
 * Description of Kpi
 *
 * @author kduret <kduret@centreon.com>
 */
class Kpi extends Centreon_Object {
    protected $table = "mod_bam_kpi";
    protected $primaryKey = "kpi_id";
    protected $uniqueLabelField = "kpi_id";

    /**
     * Get service indicators
     */
    public function getServiceIndicators()
    {
        $sql = 'SELECT k.kpi_id, k.kpi_type, CONCAT(h.host_name, " ", s.service_description) as kpi_name, b.name,'
            . 'k.drop_warning, k.drop_critical, k.drop_unknown, '
            . 'k.drop_warning_impact_id, k.drop_critical_impact_id, k.drop_unknown_impact_id '
            . 'FROM mod_bam_kpi k, mod_bam b, host h, service s '
            . 'WHERE kpi_type="0" '
            . 'AND k.host_id=h.host_id '
            . 'AND k.service_id=s.service_id '
            . 'AND k.id_ba=b.ba_id';
        return $this->getResult($sql, array(), "fetchAll");
    }

    /**
     * Get metaservice indicators
     */
    public function getMetaserviceIndicators()
    {
        $sql = 'SELECT k.kpi_id, k.kpi_type, m.meta_name as kpi_name, b.name, '
            . 'k.drop_warning, k.drop_critical, k.drop_unknown, '
            . 'k.drop_warning_impact_id, k.drop_critical_impact_id, k.drop_unknown_impact_id '
            . 'FROM mod_bam_kpi k, mod_bam b, meta_service m '
            . 'WHERE kpi_type="1" '
            . 'AND k.meta_id=m.meta_id '
            . 'AND k.id_ba=b.ba_id';
        return $this->getResult($sql, array(), "fetchAll");
    }

    /**
     * Get ba indicators
     */
    public function getBaIndicators()
    {
        $sql = 'SELECT k.kpi_id, k.kpi_type, b1.name as kpi_name, b2.name, '
            . 'k.drop_warning, k.drop_critical, k.drop_unknown, '
            . 'k.drop_warning_impact_id, k.drop_critical_impact_id, k.drop_unknown_impact_id '
            . 'FROM mod_bam_kpi k, mod_bam b1, mod_bam b2 '
            . 'WHERE kpi_type="2" '
            . 'AND k.id_indicator_ba=b1.ba_id '
            . 'AND k.id_ba=b2.ba_id';
        return $this->getResult($sql, array(), "fetchAll");
    }

    /**
     * Get boolean indicators
     */
    public function getBooleanIndicators()
    {
        $sql = 'SELECT k.kpi_id, k.kpi_type, bo.name as kpi_name, ba.name, '
            . 'k.drop_warning, k.drop_critical, k.drop_unknown, '
            . 'k.drop_warning_impact_id, k.drop_critical_impact_id, k.drop_unknown_impact_id '
            . 'FROM mod_bam_kpi k, mod_bam ba, mod_bam_boolean bo '
            . 'WHERE kpi_type="3" '
            . 'AND k.boolean_id=bo.boolean_id '
            . 'AND k.id_ba=ba.ba_id';
        return $this->getResult($sql, array(), "fetchAll");
    }

    /**
     * Get impacts
     */
    public function getImpacts()
    {
        $sql = 'SELECT id_impact, impact '
            . 'FROM mod_bam_impacts';

        $impacts = array();
        foreach ($this->getResult($sql, array(), "fetchAll") as $impact) {
            $impacts[$impact['id_impact']] = $impact['impact'];
        }

        return $impacts;
    }
    
    /**
     * 
     * @param type $iIdBa
     * @param type $iHost
     * @param type $iService
     * @return boolean
     */
    public function IsLinked($iIdBa, $iHost, $iService)
    {
        if (empty($iIdBa) || empty($iHost) || empty($iService)) {
            return true;
        }
        $bExist = true;
        
        $sql = 'SELECT kpi_id FROM mod_bam_kpi WHERE kpi_type ="0" AND host_id = :host_id AND service_id = :service_id AND id_ba = :ba_id ';
        
        $res = $this->db->query($sql, array(':ba_id' => $iIdBa, ':host_id' => $iHost , ':service_id' => $iService));
        $result = $res->fetchAll();

        if (count($result) > 0) {
            $bExist = true;
        } else {
            $bExist = false;
        }
        return $bExist;
    }

    /**
     * @param $baId
     * @param $kpiId
     * @return bool
     */
    public function isBaLinked($baId, $kpiId)
    {
        $isLinked = true;

        $sql = 'SELECT kpi_id '
            . 'FROM mod_bam_kpi '
            . 'WHERE kpi_type = "2" '
            . 'AND id_indicator_ba = :kpi_id '
            . 'AND id_ba = :ba_id ';

        $res = $this->db->query($sql, array(':ba_id' => $baId, ':kpi_id' => $kpiId));
        $result = $res->fetchAll();

        if (!count($result)) {
            $isLinked = false;
        }

        return $isLinked;
    }

    
    public function checkInfiniteLoop($kpi_ba_id, $ba_id) {
        $query = "SELECT * FROM `mod_bam_kpi` WHERE id_ba = '" . $kpi_ba_id . "' AND kpi_type = '2' ";
        $res = $this->db->query($query);
        while ($row = $res->fetch()) {
            if (isset($row['id_indicator_ba']) && $row['id_indicator_ba'] != "") {
                if ($row['id_indicator_ba'] == $ba_id || !$this->checkInfiniteLoop($row['id_indicator_ba'], $ba_id)) {
                    return false;
                }
                //return ($this->checkInfiniteLoop($row['id_indicator_ba'], $ba_id));
            }
        }
        return true;
    }
    
    /**
     * 
     * @param int $iIdBa
     * @param int $iIdMeta
     * @return boolean
     */
    public function IsMetaLinkedToBa($iIdBa, $iIdMeta)
    {
        if (empty($iIdBa) || empty($iIdMeta)) {
            return true;
        }
        $bExist = true;
        
        $sql = 'SELECT kpi_id FROM mod_bam_kpi WHERE kpi_type ="1" AND meta_id = :meta_id AND id_ba = :ba_id ';
        
        $res = $this->db->query($sql, array(':ba_id' => $iIdBa, ':meta_id' => $iIdMeta));
        $result = $res->fetchAll();

        if (count($result) > 0) {
            $bExist = true;
        } else {
            $bExist = false;
        }
        return $bExist;
    }
    
    /**
     * 
     * @param int $iIdBa
     * @param int $iIdRule
     * @return boolean
     */
    public function IsRuleLinkedToBa($iIdBa, $iIdRule)
    {
        if (empty($iIdBa) || empty($iIdRule)) {
            return true;
        }
        $bExist = true;
        
        $sql = 'SELECT kpi_id FROM mod_bam_kpi WHERE kpi_type ="3" AND boolean_id = :boolean_id AND id_ba = :ba_id ';
        
        $res = $this->db->query($sql, array(':ba_id' => $iIdBa, ':boolean_id' => $iIdRule));
        $result = $res->fetchAll();

        if (count($result) > 0) {
            $bExist = true;
        } else {
            $bExist = false;
        }
        return $bExist;
    }
}
