<?php
/*
 * MERETHIS
 *
 * Source Copyright 2005-2014 MERETHIS
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@merethis.com
 *
 */

/* init $pearDB if var is not defined */
if (!isset($pearDB)) {
	require_once $centreon_path . 'www/modules/centreon-bam-server/core/class/Centreon/Db.php';
	$pearDB = new CentreonBam_Db();
}

/* retrieve the config id of the central broker */
$query = "SELECT id "
    . "FROM nagios_server "
    . "ORDER BY localhost DESC ";
$stmt = $pearDB->query($query);
if ($stmt->numRows()) {
    $row = $stmt->fetchRow();
    $queryVirtualHost = "UPDATE host "
        . "SET host_name = '_Module_BAM_" . $row['id'] . "' "
        . "WHERE host_name like '_Module_BAM'";
    $pearDB->query($queryVirtualHost);
}

