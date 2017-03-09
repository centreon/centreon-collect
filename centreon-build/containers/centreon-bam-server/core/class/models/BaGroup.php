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
 * Description of BaGroup
 *
 * @author tmechouet
 */
class Ba_Group extends Centreon_Object {
    protected $table = "mod_bam_ba_groups";
    protected $primaryKey = "id_ba_group";
    protected $uniqueLabelField = "ba_group_name";
    
    
    /**
     * 
     * @param int $iIdBv
     * @param int $iIdBa
     */
    public function isLinked($iIdBv, $iIdBa)
    {
        if (empty($iIdBv) || empty($iIdBa)) {
            return;
        }
        $bInsert = true;
        
        $sQuery = "SELECT id_bgr from mod_bam_bagroup_ba_relation WHERE id_ba = :id_ba AND id_ba_group = :id_ba_group ";
        
        $res = $this->db->query($sQuery, array(':id_ba' => $iIdBa, ':id_ba_group' => $iIdBv));
        $result = $res->fetchAll();

        if (!count($result)) {
            $bInsert = false;
        } else {
            $bInsert = true;
        }

        return $bInsert;
    }
    /**
     * 
     * @param int $iIdBv
     * @param int $iIdBa
     * @return type
     */
    public function insertRelation($iIdBv, $iIdBa)
    {
        if (empty($iIdBv) || empty($iIdBa)) {
            return;
        }
        
        $sQueryInsert = "INSERT INTO mod_bam_bagroup_ba_relation (id_ba, id_ba_group) values(:id_ba, :id_ba_group)";
        $this->db->query($sQueryInsert, array(':id_ba' => $iIdBa, ':id_ba_group' => $iIdBv)); 
    }
    
    /**
     * 
     * @param int $iIdBv
     * @param int $iIdBa
     * @return type
     */
    public function deleteRelation($iIdBv, $iIdBa)
    {
        if (empty($iIdBv) || empty($iIdBa)) {
            return;
        }
        
        $sQueryInsert = "delete from mod_bam_bagroup_ba_relation WHERE id_ba = :id_ba AND id_ba_group = :id_ba_group";
        $this->db->query($sQueryInsert, array(':id_ba' => $iIdBa, ':id_ba_group' => $iIdBv)); 
    }

    /**
     *
     * @param int $iIdBa
     * @return type
     */
    public function deleteBaRelations($iIdBa)
    {
        if (empty($iIdBa)) {
            return;
        }

        $sQueryInsert = "delete from mod_bam_bagroup_ba_relation WHERE id_ba = :id_ba";
        $this->db->query($sQueryInsert, array(':id_ba' => $iIdBa));
    }
}
