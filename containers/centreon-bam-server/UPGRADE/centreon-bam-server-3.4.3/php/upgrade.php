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

/* init $pearDB if var is not defined */
if (!isset($pearDB)) {
    require_once $centreon_path . 'www/modules/centreon-bam-server/core/class/Centreon/Db.php';
    $pearDB = new CentreonBam_Db();
}

/* retrieve the config id of the central broker */
$sql = "SELECT config_id 
	FROM cfg_centreonbroker 
	WHERE config_filename = 'central-broker.xml'";
$stmt = $pearDB->query($sql);

/* set config id */
$configId = null;
if ($stmt->numRows()) {
    $row = $stmt->fetchRow();
    $configId = $row['config_id'];
}

/* retrieve module_id and cb_type_id */

$sql = "SELECT cb_module_id, cb_type_id
        FROM cb_type
        WHERE type_shortname = 'bam'";
$stmt = $pearDB->query($sql);
while ($row = $stmt->fetchRow()) {
    $sql = "INSERT cb_type_field_relation (cb_type_id, is_required, cb_field_id, order_display)
            (SELECT {$row['cb_type_id']}, 0, cb_field_id, @rownum := @rownum + 1
            FROM cb_field CROSS JOIN (SELECT @rownum := 0) r
            WHERE fieldname IN ('cache', 'storage_db_name')
            ORDER BY fieldname)";
    $pearDB->query($sql);
}

if (!is_null($configId)) {
    $sql = "SELECT config_group_id
        FROM cfg_centreonbroker_info
        WHERE config_key = 'type'
        AND config_value = 'bam'";
    $stmt = $pearDB->query($sql);
    if ($stmt->numRows()) {
        $row = $stmt->fetchRow();
        $configGroupId = $row['config_group_id'];
        $sql = "INSERT INTO cfg_centreonbroker_info 
		    (config_id, config_key, config_value, config_group, config_group_id) VALUES 
		    ({$configId}, 'cache', 'yes', 'output', {$configGroupId}),
		    ({$configId}, 'storage_db_name', '{$conf_centreon['dbcstg']}', 'output', {$configGroupId})";
        $pearDB->query($sql);
    }
}


/* delete double host_service_relation */

$sql = "SELECT COUNT(*) AS nbr_doublon, host_host_id, service_service_id 
FROM host_service_relation 
GROUP BY host_host_id, service_service_id 
HAVING COUNT(*) > 1";
$stmt = $pearDB->query($sql);

while ($row = $stmt->fetchRow()) {
    $sql = "DELETE
	FROM host_service_relation 
	WHERE host_host_id = " . $row['host_host_id'] . "
	AND service_service_id = " . $row['service_service_id'];
    $pearDB->query($sql);

    $sql = "INSERT INTO host_service_relation
	( host_host_id, service_service_id) VALUES (
	" . $row['host_host_id'] . ",
	" . $row['service_service_id'] . "
	)";
    $pearDB->query($sql);
}
