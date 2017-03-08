<?php
/**
 * 
 */

require_once __DIR__ . '/centAcl-Func.php';

// Get List of Business View and Acl Group relations
$bvAclList = getBusinessViewAclRelations($pearDB);

// Get Ba linked to BV
$baAclForCentreon = array();
foreach ($bvAclList as $bvAcl) {
    $basLinkedToBv = getBusinessActivitiesLinkedToBusinessView($pearDB, $bvAcl['business_view_id']);
    foreach ($basLinkedToBv as $ba) {
        $baAclForCentreon[] = array(
            'ba_id' => $ba,
            'group_id' => $bvAcl['acl_group_id']
        );
    }
}

// Get Host and Service for BA
foreach ($baAclForCentreon as &$baAcl) {
    $baAcl = array_merge($baAcl, getBaHostAndService($pearDBO, $baAcl['ba_id']));
}

// Insert into centreon ACL
$insertBaAclIntoCentreon = "INSERT INTO `centreon_acl` (`host_id` , `service_id`, `group_id`) VALUES ";
foreach($baAclForCentreon as $myBa) {
    if(isset($myBa['host_id']) && isset($myBa['service_id']) && isset($myBa['group_id'])) {
        $insertBaAclIntoCentreon .= "($myBa[host_id], $myBa[service_id], $myBa[group_id]";
        $insertBaAclIntoCentreon .= "), ";
    }
}
$insertBaAclIntoCentreon = rtrim($insertBaAclIntoCentreon, ', ');

$pearDBO->query($insertBaAclIntoCentreon);
