<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2017 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */

/* init $pearDB if var is not defined */
if (!isset($pearDB)) {
    require_once $centreon_path . 'www/modules/centreon-bam-server/core/class/Centreon/Db.php';
    $pearDB = new CentreonBam_Db();
}

/**
 *  CREATE ACL RESOURCES FROM BUSINESS VIEWS
 */

$queryInsertAclResource = "INSERT INTO "
    . "`acl_resources`(`acl_res_name`, `acl_res_alias`, `acl_res_status`, `locked`) "
    . "VALUES(?, ?, '1', 1)";
$insertAclResourcesPreparedStatement = $pearDB->prepare($queryInsertAclResource);

if (\PEAR::isError($insertAclResourcesPreparedStatement)) {
    throw new \Exception(
        $insertAclResourcesPreparedStatement->getMessage(),
        $insertAclResourcesPreparedStatement->getCode()
    );
}

$queryBusinessViews = 'SELECT ba_group_name FROM mod_bam_ba_groups';
$businessViewStmt = $pearDB->query($queryBusinessViews);

// 
$businessViewList = array();
while($rowBusinessView = $businessViewStmt->fetchRow()) {
    $ba_group_name = str_replace(' ', '_', $rowBusinessView['ba_group_name']);
    $bvAclResourceName = 'HiddenBv_' . $ba_group_name . '_DoNotDelete';
    $this->dbManager->execute($insertAclResourcesPreparedStatement, array($bvAclResourceName, $bvAclResourceName));
}
