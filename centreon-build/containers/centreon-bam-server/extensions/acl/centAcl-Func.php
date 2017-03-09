<?php
/**
 * 
 */

/**
 * 
 * @param type $db
 * @return type
 */
function getBusinessViewAclRelations($db)
{
    // Get All Bv ACL
    $queryBvAcl = "SELECT ba_group_id, acl_group_id FROM mod_bam_acl";
    $resBvAcl = $db->query($queryBvAcl);
    
    $bvAcl = array();
    if ($resBvAcl->numRows()) {
        while ($row = $resBvAcl->fetchRow()) {
            $bvAcl[] = array(
                'business_view_id' => $row['ba_group_id'],
                'acl_group_id' => $row['acl_group_id']
            );
        }
    }
    
    return $bvAcl;
}

/**
 * 
 * @param type $db
 * @param type $bvId
 * @return type
 */
function getBusinessActivitiesLinkedToBusinessView($db, $bvId)
{
    // Get all BA linked to BV
    $queryBaLinked = "SELECT id_ba FROM mod_bam_bagroup_ba_relation WHERE id_ba_group = $bvId";
    $resBas = $db->query($queryBaLinked);
    
    $bas = array();
    if ($resBas->numRows()) {
        while ($row = $resBas->fetchRow()) {
            $bas[] = $row['id_ba'];
        }
    }
    
    return $bas;
}

/**
 * 
 * @param type $dbMon
 * @param type $baId
 * @return type
 */
function getBaHostAndService($dbMon, $baId)
{
    $queryBaServiceHost = "SELECT service_id, host_id FROM services WHERE description = 'ba_$baId' AND enabled = 1";
    $baHostService = array();
    $resBaServiceHost = $dbMon->query($queryBaServiceHost);
    if ($resBaServiceHost->numRows()) {
        while ($row = $resBaServiceHost->fetchRow()) {
            $baHostService['host_id'] = $row['host_id'];
            $baHostService['service_id'] = $row['service_id'];
        }
    }
    return $baHostService;
}
